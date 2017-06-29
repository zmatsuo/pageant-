
#undef UNICODE
#include <windows.h>	// for registory
#include <stdlib.h>	// for NULL
#include <malloc.h>	// for alloca()
#include <string>
#include <vector>

#include "misc.h"
#include "ssh.h"	// base64_decode_atom()
#include "winhelp_.h"
#include "winmisc.h"
#include "misc_cpp.h"

#include "db.h"

#define APPNAME "Pageant"			// access ini and registory

#define PUTTY_DEFAULT     "Default%20Settings"
#define PUTTY_REGKEY      "Software\\SimonTatham\\PuTTY\\Sessions"

static int use_inifile;				// putty.iniを見るか?(見つけたか)
static std::string inifile_;		// putty.ini
static std::wstring inifile_w;		// putty.ini

tree234 *passphrases;


int get_use_inifile(void)
{
    return use_inifile;
}

std::string get_putty_ini()
{
	return inifile_;
}

static bool init_putty_ini_sub(const wchar_t *ini)
{
	wchar_t buf[10];
	GetPrivateProfileStringW(L"Generic", L"UseIniFile", L"", buf, _countof(buf), ini);
	return (buf[0] == L'1');
}

static void init_putty_ini()
{
	std::wstring s = _GetModuleFileName(NULL);
	s += L"\\putty.ini";
	if (init_putty_ini_sub(s.c_str())) {
		inifile_w = s;
		inifile_ = wc_to_mb(s);
		use_inifile = 1;
		return;
	}

	s = _GetCurrentDirectory();
	s += L"\\putty.ini";
	if (init_putty_ini_sub(s.c_str())) {
		inifile_w = s;
		inifile_ = wc_to_mb(s);
		use_inifile = 1;
		return;
	}

	s = _SHGetKnownFolderPath(FOLDERID_RoamingAppData);
	s += L"\\PuTTY\\putty.ini";
	if (init_putty_ini_sub(s.c_str())) {
		inifile_w = s;
        inifile_ = wc_to_mb(s);
		use_inifile = 1;
		return;
	}

	use_inifile = 0;
	inifile_w.clear();
	inifile_.clear();
}

//////////////////////////////////////////////////////////////////////////////

/* 
 * After processing a list of filenames, we want to forget the
 * passphrases.
 */
void forget_passphrases(void)
{
    while (count234(passphrases) > 0) {
		char *pp = (char *)index234(passphrases, 0);
		memset(pp, 0, strlen(pp));
		delpos234(passphrases, 0);
		free(pp);
    }
}


void save_passphrases(char* passphrase)
{
    char key[16];
    char buffer[1024];
    int i = 1;
    HKEY hkey;
    int encrypted_len = encrypto(passphrase, NULL);
    char* encrypted = (char*) alloca(encrypted_len + 1);
    encrypto(passphrase, encrypted);
    if (get_use_inifile()) {
		const char *inifile = inifile_.c_str();
		while (1) {
			sprintf(key, "crypto%d", i++);
			if (GetPrivateProfileString("Passphrases", key, "\n:", buffer, sizeof buffer, inifile) < sizeof buffer - 1) {
				if (buffer[0] == '\n') {
					WritePrivateProfileString("Passphrases", key, encrypted, inifile);
					break;
				}
				if (strcmp(buffer, encrypted) == 0)
					break;
			}
		}
		return;
    }
    if (RegCreateKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\Passphrases", &hkey) == ERROR_SUCCESS) {
        DWORD type, size;
        int ret;
        while (1) {
			sprintf(key, "crypto%d", i++);
			size = sizeof buffer;
			ret = RegQueryValueExA(hkey, key, 0, &type, (LPBYTE)buffer, &size);
			if (ret == ERROR_FILE_NOT_FOUND) {
				RegSetValueEx(hkey, key, 0, REG_SZ, (LPBYTE)encrypted, (DWORD)strlen(encrypted) + 1);
				break;
			}
			if (ret == ERROR_SUCCESS && type == REG_SZ && strcmp(buffer, encrypted) == 0)
				break;
        }
        RegCloseKey(hkey);
    }
}

void load_passphrases()
{
    char key[16];
    char buffer[1024];
    int i = 1;
    HKEY hkey;
    char* original;
    int original_len;
    if (get_use_inifile()) {
		const char *inifile = inifile_.c_str();
		while (1) {
			sprintf(key, "crypto%d", i++);
			if (GetPrivateProfileString("Passphrases", key, "\n:", buffer, sizeof buffer, inifile) < sizeof buffer - 1) {
				if (buffer[0] == '\n')
					break;
				original_len = decrypto(buffer, NULL);
				original = (char*) malloc(original_len + 1);
				if (original != NULL) {
					decrypto(buffer, original);
					addpos234(passphrases, original, 0);
				}
			}
		}
		// convert from previous version data
		i = 1;
		while (1) {
			sprintf(key, "%d", i++);
			if (GetPrivateProfileString("Passphrases", key, "\n:", buffer, sizeof buffer, inifile) < sizeof buffer - 1) {
				if (buffer[0] == '\n')
					break;
				addpos234(passphrases, strdup(buffer), 0);
				save_passphrases(buffer);
				WritePrivateProfileString("Passphrases", key, NULL, inifile);
			}
		}
    } else {
		if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\Passphrases", &hkey) == ERROR_SUCCESS) {
			DWORD type, size;
			int ret;
			while (1) {
				sprintf(key, "crypto%d", i++);
				size = sizeof buffer;
				ret = RegQueryValueEx(hkey, key, 0, &type, (LPBYTE)buffer, &size);
				if (ret == ERROR_FILE_NOT_FOUND)
					break;
				if (ret == ERROR_SUCCESS && type == REG_SZ) {
					original_len = decrypto(buffer, NULL);
					original = (char*) malloc(original_len + 1);
					if (original != NULL) {
						decrypto(buffer, original);
						addpos234(passphrases, original, 0);
					}
				}
			}
			// convert from previous version data
			i = 1;
			while (1) {
				sprintf(key, "%d", i++);
				size = sizeof buffer;
				ret = RegQueryValueEx(hkey, key, 0, &type, (LPBYTE)buffer, &size);
				if (ret == ERROR_FILE_NOT_FOUND)
					break;
				if (ret == ERROR_SUCCESS && type == REG_SZ) {
					addpos234(passphrases, strdup(buffer), 0);
					save_passphrases(buffer);
					RegDeleteValue(hkey, key);
				}
			}
			RegCloseKey(hkey);
		}
	}
}

/* generate random number by MT(http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/mt19937ar.html) */

/* Period parameters */  
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static void random_mask(char* buffer, int length) {
    /* static variables of MT */
    unsigned long mt[N]; /* the array for the state vector  */
    int mti=N+1; /* mti==N+1 means mt[N] is not initialized */
    unsigned long mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    char* dst = buffer;
    unsigned long s = 0xf92fedb5UL;
    /* initializes mt[N] with a seed */
    /* void init_genrand(unsigned long s) */
    {
	mt[0]= s & 0xffffffffUL;
	for (mti=1; mti<N; mti++) {
	    mt[mti] = 
		(1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
	    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
	    /* In the previous versions, MSBs of the seed affect   */
	    /* only MSBs of the array mt[].                        */
	    /* 2002/01/09 modified by Makoto Matsumoto             */
	    mt[mti] &= 0xffffffffUL;
	    /* for >32 bit machines */
	}
    }
    while (length-- > 0) {
	unsigned long y;
	/* generates a random number on [0,0xffffffff]-interval */
	/* unsigned long genrand_int32(void) */
	{
	    /* unsigned long y; */
	    /* static unsigned long mag01[2]={0x0UL, MATRIX_A}; */
	    /* mag01[x] = x * MATRIX_A  for x=0,1 */

	    if (mti >= N) { /* generate N words at one time */
		int kk;

		/* if (mti == N+1)   /* if init_genrand() has not been called, */
		    /* init_genrand(5489UL); /* a default initial seed is used */

		for (kk=0;kk<N-M;kk++) {
		    y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
		    mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		for (;kk<N-1;kk++) {
		    y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
		    mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
		mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

		mti = 0;
	    }
  
	    y = mt[mti++];

	    /* Tempering */
	    y ^= (y >> 11);
	    y ^= (y << 7) & 0x9d2c5680UL;
	    y ^= (y << 15) & 0xefc60000UL;
	    y ^= (y >> 18);

	    /* return y; */
	}
        *dst++ ^= (y >> 4);
    }
}

int encrypto(char* original, char* buffer)
{
    int length = (int)strlen(original);
    if (buffer != NULL) {
	random_mask(original, length);
	while (length > 0) {
	    int n = (length < 3 ? length : 3);
	    base64_encode_atom((unsigned char *)original, n, buffer);
	    original += n;
	    length -= n;
	    buffer += 4;
	}
	*buffer = '\0';
    }
    return (length + 2) / 3 * 4;
}

int decrypto(const char* encrypted, char* buffer)
{
    int encrypted_len = (int)strlen(encrypted);
    if (buffer == NULL) {
        return encrypted_len * 3 / 4;
    }else{
	const char* src = encrypted;
	const char* src_end = encrypted + encrypted_len;
	char* dst = buffer;
	while (src < src_end) {
	    int k = base64_decode_atom((char*) src, (unsigned char *)dst);
	    src += 4;
	    dst += k;
	}
        random_mask(buffer, dst - buffer);
	*dst = '\0';
	return dst - buffer;
    }
}


/**
 * @param[in]	keyname
 * @param[out]	value	"accept" or "refuce" or "\0"
 */
void get_confirm_info(const char *keyname, char *value_ptr, size_t value_size)
{
    if (keyname == NULL) {
		value_ptr[0] = '\0';
		return;
    }

    if (get_use_inifile()) {
		const char *inifile = inifile_.c_str();
		char null = '\0';
		GetPrivateProfileString(APPNAME, keyname, &null, value_ptr, value_size, inifile);
    } else {
		HKEY hkey;
		if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
			DWORD type;
			DWORD size = value_size;
			if (RegQueryValueEx(hkey, keyname, 0, &type, (LPBYTE) value_ptr, &size) != ERROR_SUCCESS || type != REG_SZ)
				value_ptr[0] = '\0';
			RegCloseKey(hkey);
		}
    }
}

/**
 * @param[in]	keyname
 * @param[out]	value	"accept" or "refuce" or "\0"
 */
void write_confirm_info(const char *keyname, const char *value)
{
    if (keyname == NULL) {
		return;
    }

    if (get_use_inifile()) {
		const char *inifile = inifile_.c_str();
		WritePrivateProfileString(APPNAME, keyname, value, inifile);
    } else {
		HKEY hkey;
		if (value != NULL) {
			if (RegCreateKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
				size_t value_length = strlen(value);
				RegSetValueEx(hkey, keyname, 0, REG_SZ, (LPBYTE)value, value_length);
				RegCloseKey(hkey);
			}
		} else {
			if (RegOpenKey(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\" APPNAME, &hkey) == ERROR_SUCCESS) {
				RegDeleteValue(hkey, keyname);
				RegCloseKey(hkey);
			}
		}
    }
}

/**
 * keyのfingerprint取得(md5)
 * Return a buffer malloced to be as big as necessary (caller must free).
 */
#if 0
char *getfingerprint(int type, const void *key)
{
    if (type == SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES ||
		type == SSH2_AGENTC_REMOVE_ALL_IDENTITIES) {
		return NULL;
    }
    if (key == NULL) {
		return NULL;
    }

    const size_t fingerprint_length = 512;
    char* fingerprint = (char*) malloc(fingerprint_length);
    fingerprint[0] = '\0';
    if (type == SSH1_AGENTC_RSA_CHALLENGE
		|| type == SSH1_AGENTC_ADD_RSA_IDENTITY
		|| type == SSH1_AGENTC_REMOVE_RSA_IDENTITY) {
		strcpy(fingerprint, "ssh1:");
		rsa_fingerprint(fingerprint + 5, fingerprint_length - 5, (struct RSAKey*) key);
    } else {
		struct ssh2_userkey* skey = (struct ssh2_userkey*) key;
//	char* fp = skey->alg->fingerprint(skey->data);
		char test[100];
		strcpy(test, "aslkj sadf adsf aeg adf");
		char* fp = test;
		strncpy(fingerprint, fp, fingerprint_length);
		size_t fp_length = strlen(fingerprint);
		if (fp_length < fingerprint_length - 2) {
			fingerprint[fp_length] = ' ';
			strncpy(fingerprint + fp_length + 1, skey->comment,
					fingerprint_length - fp_length - 1);
		}
    }
    return fingerprint;
}
#endif

/* Un-munge session names out of the registry. */
static void unmungestr(char *in, char *out, int outlen)
{
    while (*in) {
		if (*in == '%' && in[1] && in[2]) {
			int i, j;

			i = in[1] - '0';
			i -= (i > 9 ? 7 : 0);
			j = in[2] - '0';
			j -= (j > 9 ? 7 : 0);

			*out++ = (i << 4) + j;
			if (!--outlen)
				return;
			in += 3;
		} else {
			*out++ = *in++;
			if (!--outlen)
				return;
		}
    }
    *out = '\0';
    return;
}

//////////////////////////////////////////////////////////////////////////////

// puttyのsession一覧の読み込み
std::vector<std::string> setting_get_putty_sessions()
{
    static const char HEADER[] = "Session:";
    std::vector<std::string> resultAry;
    
    if (get_use_inifile()) {
		const char *inifile = inifile_.c_str();
		char* buffer;
		int length = 256;
		char* p;
		while (1) {
			buffer = snewn(length, char);
			if (GetPrivateProfileSectionNames(buffer, length, inifile) < (DWORD) length - 2)
				break;
            sfree(buffer);
			length += 256;
		}
		p = buffer;

		std::string s;
		while(*p != '\0') {
			if (!strncmp(p, HEADER, sizeof (HEADER) - 1)) {
				if(strcmp(p, PUTTY_DEFAULT) != 0) {
					char session_name[MAX_PATH + 1];
					unmungestr(p + sizeof (HEADER) - 1, session_name, MAX_PATH);
					s = session_name;
					resultAry.push_back(s);
                }
            }
            p += strlen(p) + 1;
        }
		sfree(buffer);
		return resultAry;
    } else {
		char buf[MAX_PATH + 1];
		HKEY hkey;
		if(ERROR_SUCCESS != RegOpenKeyA(HKEY_CURRENT_USER, PUTTY_REGKEY, &hkey))
			return resultAry;

		int index_key = 0;
		while(ERROR_SUCCESS == RegEnumKey(hkey, index_key, buf, MAX_PATH)) {
			TCHAR session_name[MAX_PATH + 1];
			unmungestr(buf, session_name, MAX_PATH);
			if(strcmp(buf, PUTTY_DEFAULT) != 0) {
				std::string s = session_name;
				resultAry.push_back(s);
			}
			index_key++;
		}

		RegCloseKey(hkey);

		return resultAry;
	}
}

void db_init()
{
    /*
     * See if we can find our Help file.
     */
    init_help();

	init_putty_ini();
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
