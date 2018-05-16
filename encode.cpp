
#include "encode.h"

#include <vector>

/**
 *	@retval	paddingの長さを含まない出力文字列長
 *		todo: miscの関数と入れ替える
 */
static size_t base64_encode_atom_2(const unsigned char *data, size_t *n, char *out)
{
	static const char base64_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	uint32_t u32;
	size_t output_len = 0;
	if (*n >= 3) {
		u32 = data[0] << 16;
		u32 |= data[1] << 8;
		u32 |= data[2];
		out[0] = base64_chars[(u32 >> 18) & 0x3F];
		out[1] = base64_chars[(u32 >> 12) & 0x3F];
		out[2] = base64_chars[(u32 >> 6) & 0x3F];
		out[3] = base64_chars[u32 & 0x3F];
		output_len = 4;
		*n -= 3;
	} else if (*n == 2) {
		u32 = data[0] << 16;
		u32 |= data[1] << 8;
		out[0] = base64_chars[(u32 >> 18) & 0x3F];
		out[1] = base64_chars[(u32 >> 12) & 0x3F];
		out[2] = base64_chars[(u32 >> 6) & 0x3F];
		out[3] = '=';
		output_len = 3;
		*n = 0;
	} else if (*n == 1) {
		u32 = data[0] << 16;
		out[0] = base64_chars[(u32 >> 18) & 0x3F];
		out[1] = base64_chars[(u32 >> 12) & 0x3F];
		out[2] = '=';
		out[3] = '=';
		output_len = 2;
		*n = 0;
	}
	return output_len;
}

std::string base64_encode(const void *data, size_t datalen, bool padding_enable)
{
	const uint8_t *src = (uint8_t *)data;
	std::vector<char> buf;
	buf.reserve(datalen*4/3 + 1);
	size_t pos = 0;
	size_t out_len = 0;
	while (datalen > 0) {
		buf.resize(pos+4);
		char *out = &buf[pos];
		out_len = base64_encode_atom_2(src, &datalen, out);
		src += 3;
		pos += out_len;
	}
	if (padding_enable)
		buf.push_back('\0');
	else {
		if (out_len == 4)
			buf.push_back('\0');
		else
			buf[pos] = '\0';		// cut padding
	}
	return &buf[0];
}

std::string base64_encode(const std::vector<uint8_t> &blob, bool padding_enable)
{
	return base64_encode(&blob[0], blob.size(), padding_enable);
}

std::string hex_encode(const uint8_t *blob, size_t blob_len, bool period)
{
	static const char hex_chars[] = "0123456789abcdef";
	size_t str_len = period ? blob_len*3-1 : blob_len*2;
	std::string str(str_len, 0);
	for (size_t i = 0; i < blob_len; i++) {
		uint8_t c = blob[i];
		if (period) {
			str[i*3+0] = hex_chars[(c >> 4) & 0x0f];
			str[i*3+1] = hex_chars[(c >> 0) & 0x0f];
			if (i < blob_len - 1) {
				str[i*3+2] = ':';
			}
		} else {
			str[i*2+0] = hex_chars[(c >> 4) & 0x0f];
			str[i*2+1] = hex_chars[(c >> 0) & 0x0f];
		}
	}
	return str;
}

std::string hex_encode(const std::vector<uint8_t> &blob, bool period)
{
	return hex_encode(&blob[0], blob.size(), period);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
