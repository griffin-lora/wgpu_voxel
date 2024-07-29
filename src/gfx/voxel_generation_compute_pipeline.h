#pragma once
#include "result.h"
#include <vulkan/vulkan_core.h>

extern VkImageView voxel_image_view;

result_t init_voxel_generation_compute_pipeline(void);
result_t encode_voxel_generation_compute_pipeline(VkCommandBuffer command_buffer);
void term_voxel_generation_compute_pipeline(void);