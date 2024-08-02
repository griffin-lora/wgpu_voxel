#pragma once
#include "result.h"
#include <cglm/types-struct.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define NUM_REGIONS 256

typedef struct {
    VkDescriptorSet descriptor_sets[3];
    VkBuffer uniform_buffer;
    VmaAllocation uniform_buffer_allocation;
    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_allocation;
    VkImage voxel_image;
    VmaAllocation voxel_image_allocation;
    VkImageView voxel_image_view;
} region_allocation_info_t;

typedef struct {
    VkDescriptorSet descriptor_set;
    VkImage voxel_image;
} region_generation_compute_pipeline_info_t;

typedef struct {
    VkDescriptorSet descriptor_set;
} region_meshing_compute_pipeline_info_t;

typedef struct {
    VkDescriptorSet descriptor_set;
    VkBuffer vertex_buffer;
} region_render_pipeline_info_t;

typedef enum {
    region_mesh_state_completed,
    region_mesh_state_await_meshing_compute,
    region_mesh_state_await_vertex_buffer_creation
} region_mesh_state_t;

extern region_mesh_state_t region_mesh_states[NUM_REGIONS];

extern region_allocation_info_t region_allocation_infos[NUM_REGIONS];
extern region_generation_compute_pipeline_info_t region_generation_compute_pipeline_infos[NUM_REGIONS];
extern region_meshing_compute_pipeline_info_t region_meshing_compute_pipeline_infos[NUM_REGIONS];
extern region_render_pipeline_info_t region_render_pipeline_infos[NUM_REGIONS];

result_t init_region_management(void);
void term_region_management(void);