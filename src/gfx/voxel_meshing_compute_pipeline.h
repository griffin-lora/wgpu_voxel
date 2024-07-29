#pragma once
#include "result.h"
#include <cglm/types-struct.h>
#include <stdalign.h>
#include <assert.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct {
    alignas(16) vec3s vertex_position;
    uint32_t vertex_index;
    uint32_t voxel_type;
} voxel_vertex_t;

static_assert(sizeof(voxel_vertex_t) % 16 == 0);

extern VkBuffer voxel_vertex_buffer;

result_t init_voxel_meshing_compute_pipeline(void);
result_t encode_voxel_meshing_compute_pipeline(VkCommandBuffer command_buffer);
void term_voxel_meshing_compute_pipeline(void);