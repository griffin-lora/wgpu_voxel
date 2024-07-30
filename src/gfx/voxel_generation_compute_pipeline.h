#pragma once
#include "result.h"
#include <vulkan/vulkan.h>

extern VkImageView voxel_image_view;

result_t init_voxel_generation_compute_pipeline(void);
result_t record_voxel_generation_compute_pipeline(VkCommandBuffer command_buffer);
void term_voxel_generation_compute_pipeline(void);