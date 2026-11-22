#pragma once

#include <stdint.h>

uint8_t* sof2_game_base(void);
bool sof2_is_sof2mp_exe(void);
bool sof2_mp103_matches(uint8_t* base);
