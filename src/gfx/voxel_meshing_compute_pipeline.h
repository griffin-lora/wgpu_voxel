#pragma once
#include "result.h"
#include <cglm/types-struct.h>
#include <dawn/webgpu.h>
#include <stdalign.h>
#include <assert.h>
#include <stdint.h>

typedef struct {
    alignas(16) vec3s vertex_position;
    uint32_t vertex_index;
    uint32_t voxel_type;
} voxel_vertex_t;

static_assert(sizeof(voxel_vertex_t) % 16 == 0);

extern WGPUBuffer voxel_vertex_buffer;

result_t init_voxel_meshing_compute_pipeline(void);
result_t run_voxel_meshing_compute_pipeline(void);
void term_voxel_meshing_compute_pipeline(void);