
#pragma once

#include <string>
#include <vector>
#include <stdint.h>

std::string base64_encode(const void *data, size_t datalen, bool padding_enable);
std::string base64_encode(const std::vector<uint8_t> &blob, bool padding_enable);
std::string hex_encode(const uint8_t *blob, size_t blob_len, bool period);
std::string hex_encode(const std::vector<uint8_t> &blob, bool period);
