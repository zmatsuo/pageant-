/**
   keystore.cpp

   Copyright (c) 2018 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#include "keystore.h"

#include <assert.h>
#include <algorithm>

//#define ENABLE_DEBUG_PRINT
#include "ssh.h"
#include "ckey.h"
#include "debug.h"
#include "encode.h"

static std::vector<ckey> ssh2keys_;
static std::vector<KeystoreListener *> listeners;

static void callListeners()
{
	// TODO: lock
	for (auto listener : listeners) {
		listener->change();
	}
}

void keystoreRegistListener(KeystoreListener *listener)
{
	listeners.push_back(listener);
}

void keystoreUnRegistListener(KeystoreListener *listener)
{
	listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

bool keystore_add(const ckey &key)
{
	if (keystore_exist(key)) {
		return false;
	}
	ckey key_copy = key;		// TODO copy check!
	ssh2keys_.push_back(key_copy);
	callListeners();
	return true;
}

static void clear_keys()
{
	ssh2keys_.clear();
}

void keystore_remove_all()
{
	clear_keys();
	callListeners();
}

#if 0
static void expire()
{
	if (ssh2keys_.size() == 0)
		return;
	const time_t now = time(NULL);
	auto itr = ssh2keys_.begin();
	while (itr != ssh2keys_.end())
	{
		const time_t expiration_time = itr->expiration_time();
		if(expiration_time != 0 && expiration_time < now) {
			itr = ssh2keys_.erase(itr);
		} else {
			itr++;
		}
	}
}
#endif
static void expire()
{
	if (ssh2keys_.size() == 0)
		return;
	const time_t now = time(NULL);
	auto end = std::remove_if(
		ssh2keys_.begin(), ssh2keys_.end(),
		[=](const ckey &key)->bool {
			const time_t expiration_time = key.expiration_time();
			if(expiration_time != 0 && expiration_time < now) {
				return true;
			}
			return false;
		});
	ssh2keys_.erase(end, ssh2keys_.end());
}

static int search_sha256(const char *sha256)
{
	expire();
	int idx = 0;
	for (const auto &key : ssh2keys_) {
		std::string fingerprint_sha256 = key.fingerprint_sha256();
		if (fingerprint_sha256 == sha256) {
			return idx;
		}
		idx++;
	}
	return -1;
}

static int search_public_blob(const std::vector<uint8_t> &public_blob)
{
	expire();
	int idx = 0;
	for (const auto &key : ssh2keys_) {
		auto blob = key.public_blob();
		if (public_blob.size() == blob.size() &&
			memcmp(&public_blob[0], &blob[0], public_blob.size()) == 0)
		{
			return idx;
		}
		idx++;
	}
	return -1;
}

static int search_key(const struct ssh2_userkey *skey)
{
	expire();
	int idx = 0;
	ckey a_key = ckey::create(skey);
	for (const auto &key : ssh2keys_) {
		if (a_key == key) {
			return idx;
		}
		idx++;
	}
	return -1;
}

bool keystore_get_from_blob(const std::vector<uint8_t> &blob, ckey &key)
{
	int idx = search_public_blob(blob);
	if (idx == -1) {
		key.clear();
		return false;
	}
	key = ssh2keys_[idx];
	return true;
}

bool keystore_get(const ckey &public_key, ckey &key)
{
	std::vector<uint8_t> blob_v = public_key.public_blob();
	bool r = keystore_get_from_blob(blob_v, key);
	return r;
}

void keystore_init()
{
}

void keystore_exit()
{
	clear_keys();
}

std::vector<ckey> keystore_get_keys()
{
	expire();
	std::vector<ckey> keys;
	for (auto key : ssh2keys_) {
		keys.emplace_back(key);
	}

	return keys;
}

bool keystore_get(const char *fingerprint, ckey &key)
{
	int idx = search_sha256(fingerprint);
	if (idx == -1) {
		key.clear();
		return false;
	}
	key = ssh2keys_[idx];
	return true;
}

bool keystore_remove(const char *fingerprint)
{
	int idx = search_sha256(fingerprint);
	if (idx == -1) {
		return false;
	}
	ssh2keys_.erase(ssh2keys_.begin() + idx);
	callListeners();
	return true;
}

bool keystore_remove(const ckey &key)
{
	int idx = search_key(key.get());
	if (idx == -1) {
		return false;
	}
	ssh2keys_.erase(ssh2keys_.begin() + idx);
	callListeners();
	return true;
}

// すでに存在している?
bool keystore_exist(const ckey &key)
{
	for (const auto &k : ssh2keys_) {
		if (k == key) {
			return true;
		}
	}

	return false;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
