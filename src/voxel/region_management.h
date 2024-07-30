#pragma once
#include "result.h"
#include <cglm/types-struct.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define NUM_REGIONS 16

typedef struct {
    VkDescriptorSet generation_compute_pipeline_descriptor_set;
    VkDescriptorSet render_pipeline_descriptor_set;
    VkBuffer uniform_buffer;
    VmaAllocation uniform_buffer_allocation;
    VkImage voxel_image;
    VmaAllocation voxel_image_allocation;
    VkImageView voxel_image_view;
} region_allocation_info_t;

typedef struct {
    VkDescriptorSet generation_compute_pipeline_descriptor_set;
    VkImage voxel_image;
} region_generation_compute_pipeline_info_t;

typedef struct {
    VkDescriptorSet render_descriptor_set;
} region_render_pipeline_info_t;

region_allocation_info_t region_allocation_infos[NUM_REGIONS];
region_generation_compute_pipeline_info_t region_generation_compute_pipeline_infos[NUM_REGIONS];
region_render_pipeline_info_t region_render_pipeline_infos[NUM_REGIONS];

result_t init_region_management(void);
void term_region_management(void);