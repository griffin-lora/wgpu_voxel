#pragma once
#include "result.h"
#include <cglm/types-struct.h>

result_t init_voxel_render_pipeline(void);
result_t draw_voxel_render_pipeline(WGPUCommandEncoder command_encoder, WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view);
void term_voxel_render_pipeline(void);