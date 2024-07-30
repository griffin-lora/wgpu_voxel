#pragma once
#include "result.h"
#include <cglm/types-struct.h>
#include <vulkan/vulkan.h>

result_t init_region_render_pipeline(VkCommandBuffer command_buffer, VkFence command_fence, const VkPhysicalDeviceProperties* physical_device_properties);
result_t draw_region_render_pipeline(VkCommandBuffer command_buffer);
void term_region_render_pipeline(void);