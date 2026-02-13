#pragma once

#include <cstddef>
#include <cstdint>

void crc32(const void *data, size_t n_bytes, uint32_t* crc);
uint32_t crc32_for_byte(uint32_t r);
