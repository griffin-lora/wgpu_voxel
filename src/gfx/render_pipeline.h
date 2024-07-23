#pragma once
#include "result.h"
#include <dawn/webgpu.h>

result_t init_render_pipeline(void);
result_t draw_render_pipeline(WGPUCommandEncoder command_encoder, WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
void term_render_pipeline(void);