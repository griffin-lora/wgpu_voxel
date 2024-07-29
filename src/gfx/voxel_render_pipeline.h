#pragma once
#include "gfx/default.h"
#include "result.h"
#include <cglm/types-struct.h>
#include <vulkan/vulkan.h>

extern uint32_t num_voxel_vertices;
extern VkBuffer voxel_vertex_buffer;
extern VmaAllocation voxel_vertex_buffer_allocation;

result_t init_voxel_render_pipeline(VkCommandBuffer command_buffer, VkFence command_fence, const VkPhysicalDeviceProperties* physical_device_properties);
result_t draw_voxel_render_pipeline(VkCommandBuffer command_buffer);
void term_voxel_render_pipeline(void);