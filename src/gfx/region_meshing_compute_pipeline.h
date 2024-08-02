#pragma once
#include "gfx/default.h"
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
} region_vertex_t;

static_assert(sizeof(region_vertex_t) % 16 == 0);

extern VkDescriptorSetLayout region_meshing_compute_pipeline_set_layout;
extern VkSampler voxel_sampler;

result_t init_region_meshing_compute_pipeline(const VkPhysicalDeviceProperties* physical_device_properties);
result_t record_region_meshing_compute_pipeline(VkCommandBuffer command_buffer);
result_t create_vertex_buffers_for_awaiting_regions(VkCommandBuffer command_buffer, VkFence command_fence);
void term_region_meshing_compute_pipeline(void);