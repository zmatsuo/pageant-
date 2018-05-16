/*
 * pageant.cpp
 *
 * based on pageant.c from putty(pageant)
 *
 * Copyright (c) 2018 zmatsuo
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <regex>
#include <errno.h>

#include "puttymem.h"
#define ENABLE_DEBUG_PRINT
#include "ssh.h"
#include "pageant.h"
#include "pageant_.h"
#include "winutils.h"
#include "gui_stuff.h"
#include "setting.h"
#include "passphrases.h"
#include "ckey.h"
#include "debug.h"
#include "keystore.h"
#include "codeconvert.h"
#include "rdp_ssh_relay.h"

#ifdef PUTTY_CAC
extern "C" {
#include "cert/cert_common.h"
}
#endif // PUTTY_CAC

#include "bt_agent_proxy_main.h"

#define SSH_AGENT_CONSTRAIN_LIFETIME	1
#define SSH_AGENT_CONSTRAIN_CONFIRM		2
#define SSH_AGENT_CONSTRAIN_EXTENSION	3

#if defined(_DEBUG)
void dump_msg(const void *msg);		// test
#else
#define	dump_msg(p)
#endif

static bool confirm_any_request;

/*
 * We need this to link with the RSA code, because rsaencrypt()
 * pads its data with random bytes. Since we only use rsadecrypt()
 * and the signing functions, which are deterministic, this should
 * never be called.
 *
 * If it _is_ called, there is a _serious_ problem, because it
 * won't generate true random numbers. So we must scream, panic,
 * and exit immediately if that should happen.
 */
int random_byte(void)
{
    modalfatalbox("Internal error: attempt to use random numbers in Pageant");
    exit(0);
    return 0;                 /* unreachable, but placate optimiser */
}

static void plog(void *logctx, pageant_logfn_t logfn, const char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
    ;

static void plog(void *logctx, pageant_logfn_t logfn, const char *fmt, ...)
{
    /*
     * This is the wrapper that takes a variadic argument list and
     * turns it into the va_list that the log function really expects.
     * It's safe to call this with logfn==NULL, because we
     * double-check that below; but if you're going to do lots of work
     * before getting here (such as looping, or hashing things) then
     * you should probably check logfn manually before doing that.
     */
    if (logfn) {
        va_list ap;
        va_start(ap, fmt);
        logfn(logctx, fmt, ap);
        va_end(ap);
    }
}

static std::string make_confirm_key_str(int type, const ckey &public_key)
{
	std::string keyname;
    switch (type) {
    case SSH2_AGENTC_SIGN_REQUEST:
        keyname = "QUERY:";
        break;
    case SSH2_AGENTC_ADD_IDENTITY:
	case SSH2_AGENTC_ADD_ID_CONSTRAINED:
        keyname = "ADD:";
        break;
    case SSH2_AGENTC_REMOVE_IDENTITY:
        keyname = "REMOVE:";
        break;
    case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
        keyname = "SSH2_REMOVE_ALL";
        break;
    default:
		;
    }

	if (type != SSH2_AGENTC_REMOVE_ALL_IDENTITIES)
	{
		keyname += public_key.fingerprint_md5_comp();
		keyname += " ";
		keyname += public_key.key_comment();
    }

	return keyname;
}


/**
 *	@retval		false	refused
 * 	@retval		true	accepted
 */
static bool accept_agent_request(int type, const ckey &public_key)
{
	static const char VALUE_ACCEPT[] = "accept";
    static const char VALUE_REFUSE[] = "refuse";

	std::string keyname = make_confirm_key_str(type, public_key);

	if (!public_key.get_confirmation_required()) {
		std::string value;
		setting_get_confirm_info(keyname.c_str(), value);
		if (value == VALUE_ACCEPT) {
			if (!confirm_any_request) {
				return true;
			}
		}
		else if (value == VALUE_REFUSE) {
			if (!confirm_any_request) {
				return false;
			}
		}
	}

	std::string message;
    {
		switch (type) {
		case SSH2_AGENTC_SIGN_REQUEST:
			message = "Accept query of the following key?";
			break;
		case SSH2_AGENTC_ADD_IDENTITY:
		case SSH2_AGENTC_ADD_ID_CONSTRAINED:
			message = "Accept addition of the following key?";
			break;
		case SSH2_AGENTC_REMOVE_IDENTITY:
			message = "Accept deletion of the following key?";
			break;
		case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
			message = "Accept deletion of all SSH2 identities?";
			break;
		default:
			message = "??";
			break;
		}
    }
	message += "\n";
	message += public_key.fingerprint_md5_comp();
	message += public_key.key_comment();
    struct ConfirmAcceptDlgInfo info;
    info.title = "pageant+";
    info.fingerprint = message.c_str();
    info.dont_ask_again_available =
		confirm_any_request && !public_key.get_confirmation_required() ? 1 : 0;
    info.dont_ask_again = 0;
    info.timeout = setting_get_confirm_timeout();
    DIALOG_RESULT_T r = confirmAcceptDlg(&info);

	if (info.dont_ask_again && !public_key.get_confirmation_required()) {
		const char *v = (r == DIALOG_RESULT_CANCEL) ?
			VALUE_REFUSE : VALUE_ACCEPT;
		setting_write_confirm_info(keyname.c_str(), v);
	}

	return r == DIALOG_RESULT_CANCEL ? false : true;
}

static std::vector<uint8_t> make_pubkey_blob(std::vector<ckey> keys)
{
	std::vector<uint8_t> blob(4);
	PUT_32BIT(&blob[0], keys.size());
	for (const auto &key : keys)
	{
		auto public_blob = key.public_blob();
		std::vector<uint8_t> length(4);
		PUT_32BIT(&length[0], public_blob.size());
		blob.insert(blob.end(), length.begin(), length.end());
		blob.insert(blob.end(), public_blob.begin(), public_blob.end());

		auto comment = key.key_comment();
		PUT_32BIT(&length[0], comment.size());
		blob.insert(blob.end(), length.begin(), length.end());
		blob.insert(blob.end(), comment.begin(), comment.end());
	}

    return blob;
}

static bool rfc4251_string(
	const std::vector<uint8_t> &src_blob,
	size_t &pos,
	std::vector<uint8_t> &data_blob)
{
	if (src_blob.size() < pos + 4) {
		data_blob.clear();
		return false;
	}
	size_t data_len = toint(GET_32BIT(&src_blob[pos]));
	size_t left = src_blob.size() - pos;
	left -= 4;
	if (data_len > left) {
		data_blob.clear();
		return false;
	}
	pos += 4;
	data_blob.resize(data_len);
	memcpy(&data_blob[0], &src_blob[pos], data_len);
	pos += data_len;
	return true;
}

static void rfc4251_add_string(
	const std::vector<uint8_t> &string,
	std::vector<uint8_t> &blob)
{
	uint8_t string_len[4];
	PUT_32BIT(string_len, string.size());
	blob.insert(blob.end(), &string_len[0], &string_len[4]);
	blob.insert(blob.end(), string.begin(), string.end());
}

static std::vector<uint8_t> create_agentc_sign_request(
	const std::vector<uint8_t> &public_key_blob,
	const std::vector<uint8_t> &data)
{
	std::vector<uint8_t> blob(5);
	PUT_32BIT(&blob[0], 1 + 4 + public_key_blob.size() + 4 + data.size() + 4);
	blob[4] = SSH2_AGENTC_SIGN_REQUEST;
	rfc4251_add_string(public_key_blob, blob);
	rfc4251_add_string(data, blob);
	uint8_t flags[4];
	PUT_32BIT(flags, SSH_AGENT_RSA_SHA2_512);
	blob.insert(blob.end(), &flags[0], &flags[4]);
	return blob;
}

/**
 * parse SSH2_AGENT_SIGN_RESPONSE
 */
static std::vector<uint8_t> parse_sign_response(
	const std::vector<uint8_t> &response)
{
	std::vector<uint8_t> signature;

	// 長さチェック
	if (response.size() < 5) {
		return signature;
	}

	size_t pos = 0;
	const size_t len_s = GET_32BIT(&response[pos]);
	pos += 4;
	if (response[pos] != SSH2_AGENT_SIGN_RESPONSE) {	// 0x0e
		return signature;
	}
	pos++;

	rfc4251_string(response, pos, signature);
	if (signature.size() + 4 + 1 != len_s) {
		// 長さがおかしい
		signature.clear();
	}
	return signature;
}

static bool signer(
	const ckey &public_key,
	const std::vector<uint8_t> &data,
	std::vector<uint8_t> &_signature)
{
	ckey private_key;
	keystore_get(public_key, private_key);
//	private_key.dump();
	const struct ssh2_userkey *skey = private_key.get();
	std::string comment = private_key.key_comment();

	unsigned char *signature = nullptr;
	int siglen = 0;
	if (cert_is_certpath(comment.c_str()))
	{
		const unsigned char *dataptr = &data[0];
		size_t datalen = data.size();
		signature = cert_sign(skey, (LPCBYTE)dataptr, datalen, &siglen, NULL);
		_signature.clear();
	}
	else if (strncmp("btspp://", comment.c_str(), 8) == 0)
	{
		if (setting_get_bool("bt/enable", false) == false ||
			setting_get_bool("bt/auto_connect", false) == false)
		{
			_signature.clear();
			return false;
		}

		// 接続
		std::wstring deviceName;
		std::regex re(R"(btspp://(.+)/)");
		std::smatch match;
		const std::string &f = skey->comment;
		if (std::regex_search(f, match, re)) {
			deviceName = utf8_to_wc(match[1].str());
		}

		bool r = bt_agent_proxy_main_connect(deviceName.c_str());
		if (r == false) {
			dbgprintf("接続失敗 %ls\n", deviceName.c_str());
			_signature.clear();
			return false;
		}

		// BTへ投げる
		const auto public_key_blob = private_key.public_blob();
		auto request = create_agentc_sign_request(
			public_key_blob, data);

		std::vector<uint8_t> response_v;
		r = bt_agent_proxy_main_handle_msg(request, response_v);
		if (r == false) {
			_signature.clear();
			return false;
		}
		
		// 応答チェック
		_signature = parse_sign_response(response_v);
		if (_signature.empty()) {
			return false;
		}
	}
	else if (strncmp("rdp://", comment.c_str(), 6) == 0)
	{
		if (!setting_get_bool("rdpvc-relay-server/enable", false) ||
			!rdpSshRelayCheckCom())
		{
			_signature.clear();
			return false;
		}

		// rdpへ投げる(サーバーからクライアントへ)
		std::vector<uint8_t> response;
		const auto public_key_blob = private_key.public_blob();
		auto request = create_agentc_sign_request(
			public_key_blob, data);
		bool r = rdpSshRelaySendReceive(request, response);
		if (r == false || response.size() == 0) {
			_signature.clear();
			return false;
		}

		// 応答チェック
		_signature = parse_sign_response(response);
		if (_signature.empty()) {
			return false;
		}
	}
	else
	{
		const unsigned char *dataptr = &data[0];
		size_t datalen = data.size();
		signature = skey->alg->sign(skey->data, (const char *)dataptr,
									datalen, &siglen);
		_signature.clear();
	}

	if (signature != nullptr) {
		_signature.resize(siglen);
		memcpy(&_signature[0], signature, siglen);
		sfree(signature);
	}
	return true;
}

static void *pageant_handle_msg(
	const void *msg, int msglen, int *outlen,
	void *logctx, pageant_logfn_t logfn)
{
    const unsigned char *p = (unsigned char *)msg;
    const unsigned char *msgend;
    unsigned char *ret = snewn(AGENT_MAX_MSGLEN, unsigned char);
    int type;
    const char *fail_reason = NULL;

    msgend = p + msglen;

    /*
     * Get the message type.
     */
    if (msgend < p+1) {
        fail_reason = "message contained no type code";
		goto failure;
    }
    type = *p++;

    switch (type) {
	case SSH2_AGENTC_REQUEST_IDENTITIES:
		/*
		 * Reply with SSH2_AGENT_IDENTITIES_ANSWER.
		 */
	{
		plog(logctx, logfn, "request: SSH2_AGENTC_REQUEST_IDENTITIES");
		auto keys = keystore_get_keys();
		auto blob = make_pubkey_blob(keys);
		if (blob.size() + 5 > AGENT_MAX_MSGLEN) {
			fail_reason = "output would exceed max msglen";
			goto failure;
	    }
	    PUT_32BIT(ret, blob.size() + 1);
	    ret[4] = SSH2_AGENT_IDENTITIES_ANSWER;
	    memcpy(ret + 5, &blob[0], blob.size());

		plog(logctx, logfn, "reply: SSH2_AGENT_IDENTITIES_ANSWER");
		if (logfn) {               /* skip this loop if not logging */
			for (const auto &key : keys) {
				auto fingerprint = key.fingerprint_sha1();
				plog(logctx, logfn, "returned key: %s %s",
					 fingerprint.c_str(), key.key_comment().c_str());
			}
		}
	}
	break;
	case SSH2_AGENTC_SIGN_REQUEST:
		/*
		 * Reply with either SSH2_AGENT_SIGN_RESPONSE or
		 * SSH_AGENT_FAILURE, depending on whether we have that key
		 * or not.
		 */
	{
		const std::vector<uint8_t> src(p, msgend);

		plog(logctx, logfn, "request: SSH2_AGENTC_SIGN_REQUEST");

		size_t pos = 0;

		std::vector<uint8_t> key_blob;
		bool r = rfc4251_string(src, pos, key_blob);
		if (r == false) {
			fail_reason = "request truncated before string to sign";
			goto failure;
		}

		std::vector<uint8_t> data_v;
		r = rfc4251_string(src, pos, data_v);
		if (r == false) {
			fail_reason = "request truncated before string to sign";
			goto failure;
		}

		if (src.size() < pos + 4) {
			fail_reason = "request truncated before string to sign";
			goto failure;
		}
		uint32 flags = toint(GET_32BIT(&src[pos]));
#if 0	// TODO 要調査
		if (flags != SSH_AGENT_RSA_SHA2_512) {
			fail_reason = "bad flags";
			goto failure;
		}
#endif

		//
		ckey public_key;
		r = public_key.parse_one_public_key(key_blob, &fail_reason);
		if (r == false) {
			goto failure;
		}
		public_key.dump();

		if (logfn) {
			std::string fingerprint = public_key.fingerprint_md5_comp();
			plog(logctx, logfn, "requested key: %s", fingerprint.c_str());
		}
	    if (!accept_agent_request(type, public_key)) {
			fail_reason = "refused";
			goto failure;
	    }

		std::vector<uint8_t> signature;
		r = signer(public_key, data_v, signature);
		if (r == false) {
			fail_reason = "sign error";
			goto failure;
		}

		size_t len = 5 + 4 + signature.size();
	    PUT_32BIT(ret, len - 4);
	    ret[4] = SSH2_AGENT_SIGN_RESPONSE;
	    PUT_32BIT(ret + 5, signature.size());
	    memcpy(ret + 5 + 4, &signature[0], signature.size());

		std::wstring message =
			utf8_to_wc(public_key.fingerprint_md5_comp().substr(0,30) + "...");
		showTrayMessage(
			L"Signed",
			message.c_str());

		plog(logctx, logfn, "reply: SSH2_AGENT_SIGN_RESPONSE");
	}
	break;
	case SSH2_AGENTC_ADD_IDENTITY:
	case SSH2_AGENTC_ADD_ID_CONSTRAINED:
		/*
		 * Add to the list and return SSH_AGENT_SUCCESS, or
		 * SSH_AGENT_FAILURE if the key was malformed.
		 */
	{
		plog(logctx, logfn,
			 type == SSH2_AGENTC_ADD_IDENTITY ?
			 "request: SSH2_AGENTC_ADD_IDENTITY":
			 "request: SSH2_AGENTC_ADD_ID_CONSTRAINED");
		ckey key;
		size_t pos = 0;
		const std::vector<uint8_t> blob(p, msgend+1);
		bool r = key.parse_one_private_key(blob, pos, &fail_reason);
		if (r == false) {
			goto failure;
		}
		if (type == SSH2_AGENTC_ADD_ID_CONSTRAINED) {
			size_t left = blob.size() - pos;

			bool continue_flag = true;
			bool confirm = false;
			while (continue_flag) {
				uint8_t constrain_type = blob[pos++];
				left--;
				switch (constrain_type) {
				case SSH_AGENT_CONSTRAIN_LIFETIME:
				{
					if (left < 4) {
						continue_flag = false;
						break;
					}
					uint32_t lifetime = GET_32BIT(&blob[pos]);
					key.set_lifetime(lifetime);
					pos += 4;
					left -= 4;
					break;
				}
				case SSH_AGENT_CONSTRAIN_CONFIRM:
					key.require_confirmation(true);
					break;
				//case SSH_AGENT_CONSTRAIN_EXTENSION:
				default:
					continue_flag = false;
					break;
				}
				if (left <= 0) {
					continue_flag = false;
					break;
				}
			}
		}
	    if (!accept_agent_request(type, key)) {
			fail_reason = "refused";
	        goto failure;
	    }
		if (logfn) {
			plog(logctx, logfn, "submitted key: %s %s",
				 key.fingerprint().c_str(), key.key_comment().c_str());
		}
		if (keystore_exist(key)) {
			fail_reason = "key already present";
			goto failure;
		}
		if (keystore_add(key)) {
			PUT_32BIT(ret, 1);
			ret[4] = SSH_AGENT_SUCCESS;

			plog(logctx, logfn, "reply: SSH_AGENT_SUCCESS");
	    } else {
			fail_reason = "key already present";
			goto failure;
	    }
	}
	break;
	case SSH2_AGENTC_REMOVE_IDENTITY:
		/*
		 * Remove from the list and return SSH_AGENT_SUCCESS, or
		 * perhaps SSH_AGENT_FAILURE if it wasn't in the list to
		 * start with.
		 */
	{
		std::vector<uint8_t> src(p, msgend);

		plog(logctx, logfn, "request: SSH2_AGENTC_REMOVE_IDENTITY");

		size_t pos = 0;

		std::vector<uint8_t> key_blob;
		bool r = rfc4251_string(src, pos, key_blob);
		if (r == false) {
			fail_reason = "request truncated before public key";
			goto failure;
		}
		ckey public_key;
		r = public_key.parse_one_public_key(key_blob, &fail_reason);
		if (r == false) {
			fail_reason = "bad key";
			goto failure;
		}

		if (logfn) {
			auto fingerprint = public_key.fingerprint_md5_comp();
			plog(logctx, logfn, "unwanted key: %s", fingerprint.c_str());
		}

	    if (!accept_agent_request(type, public_key)) {
			fail_reason = "refused";
	        goto failure;
	    }

		plog(logctx, logfn, "found with comment: %s", public_key.key_comment().c_str());
		r = keystore_remove(public_key);

		PUT_32BIT(ret, 1);
		ret[4] = r ? SSH_AGENT_SUCCESS : SSH_AGENT_FAILURE;

		plog(logctx, logfn, r ? "reply: SSH_AGENT_SUCCESS" : "reply: SSH_AGENT_FAILURE");
	}
	break;
	case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
		/*
		 * Remove all SSH-2 keys. Always returns success.
		 */
	{
		plog(logctx, logfn, "request: SSH2_AGENTC_REMOVE_ALL_IDENTITIES");

		keystore_remove_all();

	    PUT_32BIT(ret, 1);
	    ret[4] = SSH_AGENT_SUCCESS;

		plog(logctx, logfn, "reply: SSH_AGENT_SUCCESS");
	}
	break;
	default:
        plog(logctx, logfn, "request: unknown message type %d", type);

        fail_reason = "unrecognised message";
        /* fall through */
	failure:
		/*
		 * Unrecognised message. Return SSH_AGENT_FAILURE.
		 */
		PUT_32BIT(ret, 1);
		ret[4] = SSH_AGENT_FAILURE;
        plog(logctx, logfn, "reply: SSH_AGENT_FAILURE (%s)", fail_reason);
		break;
    }

    *outlen = 4 + GET_32BIT(ret);
    return ret;
}

static void *pageant_failure_msg(int *outlen)
{
    unsigned char *ret = snewn(5, unsigned char);
    PUT_32BIT(ret, 1);
    ret[4] = SSH_AGENT_FAILURE;
    *outlen = 5;
    return ret;
}

void pageant_init(void)
{
	keystore_init();
}

void pageant_exit(void)
{
	keystore_exit();
}

/* ----------------------------------------------------------------------
 * The agent plug.
 */
#if 0
/*
 * Coroutine macros similar to, but simplified from, those in ssh.c.
 */
#define crBegin(v)	{ int *crLine = &v; switch(v) { case 0:;
#define crFinish(z)	} *crLine = 0; return (z); }
#define crGetChar(c) do                                         \
    {                                                           \
        while (len == 0) {                                      \
            *crLine =__LINE__; return 1; case __LINE__:;        \
        }                                                       \
        len--;                                                  \
        (c) = (unsigned char)*data++;                           \
    } while (0)

struct pageant_conn_state {
    const struct plug_function_table *fn;
    /* the above variable absolutely *must* be the first in this structure */

    Socket connsock;
    void *logctx;
    pageant_logfn_t logfn;
    unsigned char lenbuf[4], pktbuf[AGENT_MAX_MSGLEN];
    unsigned len, got;
    int real_packet;
    int crLine;            /* for coroutine in pageant_conn_receive */
};

static int pageant_conn_closing(Plug plug, const char *error_msg,
                                int error_code, int calling_back)
{
    struct pageant_conn_state *pc = (struct pageant_conn_state *)plug;
    if (error_msg)
        plog(pc->logctx, pc->logfn, "%p: error: %s", pc, error_msg);
    else
        plog(pc->logctx, pc->logfn, "%p: connection closed", pc);
    sk_close(pc->connsock);
    sfree(pc);
    return 1;
}

static void pageant_conn_sent(Plug plug, int bufsize)
{
    /* struct pageant_conn_state *pc = (struct pageant_conn_state *)plug; */

    /*
     * We do nothing here, because we expect that there won't be a
     * need to throttle and unthrottle the connection to an agent -
     * clients will typically not send many requests, and will wait
     * until they receive each reply before sending a new request.
     */
}

static void pageant_conn_log(void *logctx, const char *fmt, va_list ap)
{
    /* Wrapper on pc->logfn that prefixes the connection identifier */
    struct pageant_conn_state *pc = (struct pageant_conn_state *)logctx;
    char *formatted = dupvprintf(fmt, ap);
    plog(pc->logctx, pc->logfn, "%p: %s", pc, formatted);
    sfree(formatted);
}

static int pageant_conn_receive(Plug plug, int urgent, char *data, int len)
{
    struct pageant_conn_state *pc = (struct pageant_conn_state *)plug;
    char c;

    crBegin(pc->crLine);

    while (len > 0) {
        pc->got = 0;
        while (pc->got < 4) {
            crGetChar(c);
            pc->lenbuf[pc->got++] = c;
        }

        pc->len = GET_32BIT(pc->lenbuf);
        pc->got = 0;
        pc->real_packet = (pc->len < AGENT_MAX_MSGLEN-4);

        while (pc->got < pc->len) {
            crGetChar(c);
            if (pc->real_packet)
                pc->pktbuf[pc->got] = c;
            pc->got++;
        }

        {
            void *reply;
            int replylen;

            if (pc->real_packet) {
                reply = pageant_handle_msg(pc->pktbuf, pc->len, &replylen, pc,
                                           pc->logfn?pageant_conn_log:NULL);
            } else {
                plog(pc->logctx, pc->logfn, "%p: overlong message (%u)",
                     pc, pc->len);
                plog(pc->logctx, pc->logfn, "%p: reply: SSH_AGENT_FAILURE "
                     "(message too long)", pc);
                reply = pageant_failure_msg(&replylen);
            }
            sk_write(pc->connsock, reply, replylen);
            smemclr(reply, replylen);
        }
    }

    crFinish(1);
}

struct pageant_listen_state {
    const struct plug_function_table *fn;
    /* the above variable absolutely *must* be the first in this structure */

    Socket listensock;
    void *logctx;
    pageant_logfn_t logfn;
};

static int pageant_listen_closing(Plug plug, const char *error_msg,
                                  int error_code, int calling_back)
{
    struct pageant_listen_state *pl = (struct pageant_listen_state *)plug;
    if (error_msg)
        plog(pl->logctx, pl->logfn, "listening socket: error: %s", error_msg);
    sk_close(pl->listensock);
    pl->listensock = NULL;
    return 1;
}

static int pageant_listen_accepting(Plug plug,
                                    accept_fn_t constructor, accept_ctx_t ctx)
{
    static const struct plug_function_table connection_fn_table = {
	NULL, /* no log function, because that's for outgoing connections */
	pageant_conn_closing,
        pageant_conn_receive,
        pageant_conn_sent,
	NULL /* no accepting function, because we've already done it */
    };
    struct pageant_listen_state *pl = (struct pageant_listen_state *)plug;
    struct pageant_conn_state *pc;
    const char *err;
    char *peerinfo;

    pc = snew(struct pageant_conn_state);
    pc->fn = &connection_fn_table;
    pc->logfn = pl->logfn;
    pc->logctx = pl->logctx;
    pc->crLine = 0;

    pc->connsock = constructor(ctx, (Plug) pc);
    if ((err = sk_socket_error(pc->connsock)) != NULL) {
        sk_close(pc->connsock);
        sfree(pc);
	return TRUE;
    }

    sk_set_frozen(pc->connsock, 0);

    peerinfo = sk_peer_info(pc->connsock);
    if (peerinfo) {
        plog(pl->logctx, pl->logfn, "%p: new connection from %s",
             pc, peerinfo);
    } else {
        plog(pl->logctx, pl->logfn, "%p: new connection", pc);
    }

    return 0;
}

struct pageant_listen_state *pageant_listener_new(void)
{
    static const struct plug_function_table listener_fn_table = {
        NULL, /* no log function, because that's for outgoing connections */
        pageant_listen_closing,
        NULL, /* no receive function on a listening socket */
        NULL, /* no sent function on a listening socket */
        pageant_listen_accepting
    };

    struct pageant_listen_state *pl = snew(struct pageant_listen_state);
    pl->fn = &listener_fn_table;
    pl->logctx = NULL;
    pl->logfn = NULL;
    pl->listensock = NULL;
    return pl;
}

void pageant_listener_got_socket(struct pageant_listen_state *pl, Socket sock)
{
    pl->listensock = sock;
}

void pageant_listener_set_logfn(struct pageant_listen_state *pl,
                                void *logctx, pageant_logfn_t logfn)
{
    pl->logctx = logctx;
    pl->logfn = logfn;
}

void pageant_listener_free(struct pageant_listen_state *pl)
{
    if (pl->listensock)
        sk_close(pl->listensock);
    sfree(pl);
}
#endif

/**
 * SSH2_AGENT_IDENTITIES_ANSWERを送信、
 * 結果をパースしてキー一覧にして返す
 *
 * @retval	0			ok
 * @retval	EINVAL		受信データがおかしい
 * @param	query_func	問い合わせ用関数
 * @param	keys		鍵
 */
int pageant_get_keylist(
	agent_query_synchronous_fn query_func,
	std::vector<ckey> &keys)
{
	std::vector<uint8_t> request(5);
	PUT_32BIT(&request[0], 1);
	request[4] = SSH2_AGENTC_REQUEST_IDENTITIES;

	std::vector<uint8_t> response;
	bool r = query_func(request, response);
	if (r == false) {
		keys.clear();
		return EINVAL;
	}
	if (response.size() < 5 || response[4] != SSH2_AGENT_IDENTITIES_ANSWER)
	{
		keys.clear();
		return EINVAL;
	}

	// 鍵を抽出
	const char *fail_reason = nullptr;
	r = parse_public_keys(
		&response[5], response.size() - 5,
		keys, &fail_reason);
	if (r == false) {
		dbgprintf("err %s\n", fail_reason);
		return EINVAL;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

/**
 * dump
 */
#if defined(_DEBUG)
void dump_msg(const void *msg)
{
    const unsigned char *top = (unsigned char *)msg;
    const unsigned char *p = (unsigned char *)msg;
    int length = GET_32BIT(p);

    if (length == 0) {
		dbgprintf("message length is zero\n");
		return;
    }

    int type = p[4];

    switch (type) {
    case SSH2_AGENTC_REQUEST_IDENTITIES:
		/*
		 * Reply with SSH2_AGENT_IDENTITIES_ANSWER.
		 */
		dbgprintf("SSH2_AGENTC_REQUEST_IDENTITIES(0x%x)\n", type);
		dbgprintf("msg len=0x%04x(%d)\n", length, length);
		debug_memdump(p, 4 + 1, 1);
		p += 5;
		break;
    case SSH2_AGENT_IDENTITIES_ANSWER:
    {
		dbgprintf("SSH2_AGENT_IDENTITIES_ANSWER(0x%x)\n", type);
		dbgprintf("msg len=0x%04x(%d)\n", length, length);
		debug_memdump(p, 4 + 1, 1);
		p += 5;
		int nkey = GET_32BIT(p);
		dbgprintf("key count %d\n", nkey);
		debug_memdump(p, 4, 1);
		p += 4;
		for (int i=0; i < nkey; i++) {
			dbgprintf("%d/%d\n", i, nkey);
			int blob_len = GET_32BIT(p);
			debug_memdump(p, 4, 1);
			p += 4;
			dbgprintf(" blob\n");
			debug_memdump(p, blob_len, 1);
			p += blob_len;
			int comment_len = GET_32BIT(p);
			debug_memdump(p, 4, 1);
			p += 4;
			dbgprintf(" comment\n");
			debug_memdump(p, comment_len, 1);
			p += comment_len;
		}
		break;
    }
    case SSH2_AGENTC_SIGN_REQUEST:
    {
		/*
		 * Reply with either SSH2_AGENT_SIGN_RESPONSE or
		 * SSH_AGENT_FAILURE, depending on whether we have that key
		 * or not.
		 */
		dbgprintf("SSH2_AGENTC_SIGN_REQUEST(0x%x)\n", type);
		dbgprintf("msg len=0x%04x(%d)\n", length, length);
		debug_memdump(p, 4 + 1, 1);
		p += 5;
		int blob_len = GET_32BIT(p);
		dbgprintf(" blob len=0x%04x(%d)\n", blob_len, blob_len);
		debug_memdump(p, 4, 1);
		p += 4;
		debug_memdump(p, blob_len, 1);
		p += blob_len;
		int data_len = GET_32BIT(p);
		dbgprintf(" data len=0x%04x(%d)\n", data_len, data_len);
		debug_memdump(p, 4, 1);
		p += 4;
		debug_memdump(p, data_len, 1);
		p += data_len;
		break;
    }
    case SSH2_AGENT_SIGN_RESPONSE:
    {
		dbgprintf("SSH2_AGENT_SIGN_RESPONSE(0x%x)\n", type);
		dbgprintf("msg len=0x%04x(%d)\n", length, length);
		debug_memdump(p, 4 + 1, 1);
		p += 5;
		int sign_len = GET_32BIT(p);
		debug_memdump(p, 4, 1);
		p += 4;
		dbgprintf(" sign data len=0x%04x(%d)\n", sign_len, sign_len);
		debug_memdump(p, sign_len, 1);
		p += sign_len;
		break;
    }
    case SSH2_AGENTC_ADD_IDENTITY:
	case SSH2_AGENTC_ADD_ID_CONSTRAINED:
	{
		dbgprintf(
			"%s(%d(0x%x))\n",
			type == SSH2_AGENTC_ADD_IDENTITY ? "SSH2_AGENTC_ADD_IDENTITY":
			type == SSH2_AGENTC_ADD_ID_CONSTRAINED ? "SSH2_AGENTC_ADD_ID_CONSTRAINED" :
			"?",
			type, type);
		dbgprintf("msg len=0x%04x(%d)\n", length, length);
		debug_memdump(p, 4 + 1, 1);
		p += 5;

		const unsigned char *msgend = p + length;

		const char *fail_reason = NULL;
		size_t key_len;
		ckey key;
		bool r = key.parse_one_private_key(
			p,  msgend - p, &fail_reason, &key_len);
		if (r == false) {
			// TODO
			break;
		}
		debug_memdump(p, key_len, 1);
		p += key_len;

		if (type == SSH2_AGENTC_ADD_ID_CONSTRAINED) {
			size_t left = msgend - p;
			debug_memdump(p, left, 1);

			bool continue_flag = true;
			uint32_t lifetime = 0;
			bool confirm = false;
			while (continue_flag) {
				uint8_t constrain_type = *p++;
				left--;
				switch (constrain_type) {
				case SSH_AGENT_CONSTRAIN_LIFETIME:
					if (left < 4) {
						continue_flag = false;
						break;
					}
					lifetime = GET_32BIT(p);
					p += 4;
					left -= 4;
					break;
				case SSH_AGENT_CONSTRAIN_CONFIRM:
					confirm = true;
					break;
				//case SSH_AGENT_CONSTRAIN_EXTENSION:
				default:
					continue_flag = false;
					break;
				}
				if (left <= 0) {
					continue_flag = false;
					break;
				}
			}
			p--;
#if 0
			const int constraints_len = GET_32BIT(p);
			dbgprintf(" constraints_len=%d\n", constraints_len);
			debug_memdump(p, 4, 1);
			p += 4;
			debug_memdump(p, constraints_len, 1);
			p += constraints_len;
#endif
		}
		break;
	}
    ///////////////////
#if 0
    case SSH1_AGENTC_REQUEST_RSA_IDENTITIES:
    case SSH1_AGENTC_RSA_CHALLENGE:
    case SSH1_AGENTC_ADD_RSA_IDENTITY:
		/*
		 * Add to the list and return SSH_AGENT_SUCCESS, or
		 * SSH_AGENT_FAILURE if the key was malformed.
		 */
    case SSH1_AGENTC_REMOVE_RSA_IDENTITY:
		/*
		 * Remove from the list and return SSH_AGENT_SUCCESS, or
		 * perhaps SSH_AGENT_FAILURE if it wasn't in the list to
		 * start with.
		 */
#endif
    case SSH2_AGENTC_REMOVE_IDENTITY:
		/*
		 * Remove from the list and return SSH_AGENT_SUCCESS, or
		 * perhaps SSH_AGENT_FAILURE if it wasn't in the list to
		 * start with.
		 */
#if 0
    case SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
		/*
		 * Remove all SSH-1 keys. Always returns success.
		 */
#endif
    case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
		/*
		 * Remove all SSH-2 keys. Always returns success.
		 */
    default:
		/*
		 * Unrecognised message. Return SSH_AGENT_FAILURE.
		 */
		dbgprintf("type 0x%02x\n", type);
		dbgprintf("msg len=%d(0x%x)\n", length, length);
		debug_memdump(p, 4 + 1, 1);
		p += 5;
		if (length > 1) {
			int dump_len = length;
#if 0
//#define AGENT_MAX_MSGLEN  8192
			if (dump_len > 0x80) {
				dbgprintf("len is too large, clip 0x80\n");
				dump_len = 0x80;
			}
#endif
			debug_memdump(p, dump_len, 1);
			p += length;
		}
		break;
    }

    int actual_length = (int)(uintptr_t)(p - top) - 4;
    if (actual_length != length) {
		debug_printf("  last adr %p\n", p);
		debug_printf("  message length 0x%04x(%d)\n", length, length);
		debug_printf("  actual  length 0x%04x(%d)\n", actual_length, actual_length);
		debug_printf("  length  %s\n",
			  length == actual_length ? "ok" :
			  length < actual_length ? "message is small? dangerous!" :
			  "message is large garbage on back?");
		if (length > actual_length) {
			debug_memdump(p, length - actual_length, 1);
		}
    }
}
#else
#define	dump_msg(p)
#endif

static void pageant_logfn_test(void *logctx, const char *fmt, va_list ap)
{
	(void)logctx;

	std::string buf(512, 0);
    int len = vsnprintf(&buf[0], buf.size(), fmt, ap);
	if (len < 0) {
		dbgprintf("logtest format error\n");
		return;
	}
	dbgprintf("logtest '%s'\n", buf.c_str());
}

// replyは smemclr(), sfree() すること
#if 1
void *pageant_handle_msg_2(const void *msgv, int *_replylen)
{
    dbgprintf("answer_msg enter --\n");

    unsigned char *msg = (unsigned char *)msgv;
    unsigned msglen;
    void *reply;
    int replylen;

    dump_msg(msg);

    msglen = GET_32BIT(msg);
    if (msglen > AGENT_MAX_MSGLEN) {
        reply = pageant_failure_msg(&replylen);
    } else {
		pageant_logfn_t logfn = pageant_logfn_test;
        reply = pageant_handle_msg(msg + 4, msglen, &replylen, NULL, logfn);
        if (replylen > AGENT_MAX_MSGLEN) {
            smemclr(reply, replylen);
            sfree(reply);
            reply = pageant_failure_msg(&replylen);
        }
    }

    dump_msg(reply);
    dbgprintf("answer_msg leave --\n");

    *_replylen = replylen;
    return reply;
}
#endif

void set_confirm_any_request(bool _bool)
{
    confirm_any_request = _bool;
}

int get_confirm_any_request(void)
{
    return confirm_any_request;
}

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
