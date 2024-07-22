#pragma once
#include "result.h"

result_t init_gfx();
bool should_window_close();
result_t draw_gfx();
void term_gfx();