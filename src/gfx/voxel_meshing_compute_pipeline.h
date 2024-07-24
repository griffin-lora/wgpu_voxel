#pragma once
#include "result.h"
#include <cglm/types-struct.h>
#include <dawn/webgpu.h>

typedef struct {
    vec3s position;
    vec2s texel_coord;
} __attribute__((aligned (16))) voxel_vertex_t;

extern WGPUBuffer voxel_vertex_buffer;

result_t init_voxel_meshing_compute_pipeline(void);
result_t run_voxel_meshing_compute_pipeline(void);
void term_voxel_meshing_compute_pipeline(void);