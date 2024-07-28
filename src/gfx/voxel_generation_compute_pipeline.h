#pragma once
#include "result.h"

extern WGPUTextureView voxel_texture_view;

result_t init_voxel_generation_compute_pipeline(void);
result_t run_voxel_generation_compute_pipeline(void);
void term_voxel_generation_compute_pipeline(void);