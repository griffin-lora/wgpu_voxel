#include "region_management.h"
#include "gfx/default.h"
#include "gfx/gfx.h"
#include "gfx/region_generation_compute_pipeline.h"
#include "gfx/region_meshing_compute_pipeline.h"
#include "gfx/region_render_pipeline.h"
#include "result.h"
#include "voxel/region.h"
#include <vulkan/vulkan_core.h>

region_mesh_state_t region_mesh_states[NUM_REGIONS];

region_allocation_info_t region_allocation_infos[NUM_REGIONS];
region_generation_compute_pipeline_info_t region_generation_compute_pipeline_infos[NUM_REGIONS];
region_meshing_compute_pipeline_info_t region_meshing_compute_pipeline_infos[NUM_REGIONS];
region_render_pipeline_info_t region_render_pipeline_infos[NUM_REGIONS];

static VkDescriptorPool descriptor_pool;

typedef struct {
    ivec3s region_position;
} region_uniform_t;

result_t init_region_management(void) {
    if (vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 3,
        .pPoolSizes = (VkDescriptorPoolSize[3]) {
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1
            },
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1
            }
        },
        .maxSets = NUM_REGIONS * 3
    }, NULL, &descriptor_pool) != VK_SUCCESS) {
        return result_descriptor_pool_create_failure;
    }

    for (size_t region_index = 0; region_index < NUM_REGIONS; region_index++) {
        region_allocation_info_t* allocation_info = &region_allocation_infos[region_index];

        // Set these to NULL so we don't accidentally destroy NULL buffers later
        allocation_info->vertex_buffer = NULL;
        allocation_info->vertex_buffer_allocation = NULL;

        if (vmaCreateImage(allocator, &(VkImageCreateInfo) {
            DEFAULT_VK_IMAGE,
            .imageType = VK_IMAGE_TYPE_3D,
            .format = VK_FORMAT_R8_UINT,
            .extent = { REGION_SIZE, REGION_SIZE, REGION_SIZE },
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        }, &device_allocation_create_info, &allocation_info->voxel_image, &allocation_info->voxel_image_allocation, NULL) != VK_SUCCESS) {
            return result_image_create_failure;
        }

        if (vkCreateImageView(device, &(VkImageViewCreateInfo) {
            DEFAULT_VK_IMAGE_VIEW,
            .image = allocation_info->voxel_image,
            .viewType = VK_IMAGE_VIEW_TYPE_3D,
            .format = VK_FORMAT_R8_UINT,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
        }, NULL, &allocation_info->voxel_image_view) != VK_SUCCESS) {
            return result_image_view_create_failure;
        }

        if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
            DEFAULT_VK_UNIFORM_BUFFER,
            .size = sizeof(region_uniform_t)
        }, &shared_write_allocation_create_info, &allocation_info->uniform_buffer, &allocation_info->uniform_buffer_allocation, NULL) != VK_SUCCESS) {
            return result_buffer_create_failure;
        }
        
        region_uniform_t* uniform;
        if (vmaMapMemory(allocator, allocation_info->uniform_buffer_allocation, (void*) &uniform) != VK_SUCCESS) {
            return result_memory_map_failure;
        }
        
        *uniform = (region_uniform_t) {
            .region_position = (ivec3s) {{ (int32_t) REGION_SIZE * (int32_t) (region_index / 16), 0, (int32_t) REGION_SIZE * (int32_t) (region_index % 16) }}
        };

        vmaUnmapMemory(allocator, allocation_info->uniform_buffer_allocation);

        if (vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 3,
            .pSetLayouts = (VkDescriptorSetLayout[3]) {
                region_generation_compute_pipeline_set_layout,
                region_meshing_compute_pipeline_set_layout,
                region_render_pipeline_set_layout
            }
        }, allocation_info->descriptor_sets) != VK_SUCCESS) {
            return result_descriptor_sets_allocate_failure;
        }

        vkUpdateDescriptorSets(device, 4, (VkWriteDescriptorSet[4]) {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = allocation_info->descriptor_sets[0],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .pImageInfo = &(VkDescriptorImageInfo) {
                    .imageView = allocation_info->voxel_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL
                }
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = allocation_info->descriptor_sets[0],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = allocation_info->uniform_buffer,
                    .offset = 0,
                    .range = sizeof(region_uniform_t)
                }
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = allocation_info->descriptor_sets[1],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &(VkDescriptorImageInfo) {
                    .sampler = voxel_sampler,
                    .imageView = allocation_info->voxel_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                }
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = allocation_info->descriptor_sets[2],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = allocation_info->uniform_buffer,
                    .offset = 0,
                    .range = sizeof(region_uniform_t)
                }
            }
        }, 0, NULL);

        region_mesh_states[region_index] = region_mesh_state_await_meshing_compute;

        region_generation_compute_pipeline_info_t* generation_compute_pipeline_info = &region_generation_compute_pipeline_infos[region_index];
        region_meshing_compute_pipeline_info_t* meshing_compute_pipeline_info = &region_meshing_compute_pipeline_infos[region_index];
        region_render_pipeline_info_t* render_pipeline_info = &region_render_pipeline_infos[region_index];

        *generation_compute_pipeline_info = (region_generation_compute_pipeline_info_t) {
            .descriptor_set = allocation_info->descriptor_sets[0],
            .voxel_image = allocation_info->voxel_image
        };
        *meshing_compute_pipeline_info = (region_meshing_compute_pipeline_info_t) {
            .descriptor_set = allocation_info->descriptor_sets[1]
        };
        *render_pipeline_info = (region_render_pipeline_info_t) {
            .descriptor_set = allocation_info->descriptor_sets[2],
            .vertex_buffer = NULL
        };
    }

    return result_success;
}

void term_region_management(void) {
    vkDestroyDescriptorPool(device, descriptor_pool, NULL);

    for (size_t region_index = 0; region_index < NUM_REGIONS; region_index++) {
        region_allocation_info_t* allocation_info = &region_allocation_infos[region_index];

        vmaDestroyBuffer(allocator, allocation_info->uniform_buffer, allocation_info->uniform_buffer_allocation);
        if (allocation_info->vertex_buffer != NULL && allocation_info->vertex_buffer_allocation != NULL) {
            vmaDestroyBuffer(allocator, allocation_info->vertex_buffer, allocation_info->vertex_buffer_allocation);
        }
        vkDestroyImageView(device, allocation_info->voxel_image_view, NULL);
        vmaDestroyImage(allocator, allocation_info->voxel_image, allocation_info->voxel_image_allocation);
    }
}