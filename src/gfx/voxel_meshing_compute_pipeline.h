#pragma once
#include "result.h"
#include <cglm/types-struct.h>

typedef struct {
    vec3s position;
    vec2s texel_coord;
} voxel_vertex_t;

result_t init_voxel_meshing_compute_pipeline(void);
result_t run_voxel_meshing_compute_pipeline(void);
void term_voxel_meshing_compute_pipeline(void);