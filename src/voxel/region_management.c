#include "region_management.h"
#include "gfx/default.h"
#include "gfx/gfx.h"
#include "result.h"
#include "voxel/region.h"
#include <vulkan/vulkan_core.h>

typedef struct {
    ivec3s region_position;
} region_uniform_t;

result_t init_region_management(void) {
    for (size_t region_index = 0; region_index < NUM_REGIONS; region_index++) {
        region_allocation_info_t* allocation_info = &region_allocation_infos[region_index];

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
            .region_position = (ivec3s) {{ 16 * (int32_t) region_index, 0, 0 }}
        };

        vmaUnmapMemory(allocator, allocation_info->uniform_buffer_allocation);

        region_generation_compute_pipeline_info_t* generation_compute_pipeline_info = &region_generation_compute_pipeline_infos[region_index];
        region_render_pipeline_info_t* render_pipeline_info = &region_render_pipeline_infos[region_index];

        *generation_compute_pipeline_info = (region_generation_compute_pipeline_info_t) {
            .voxel_image = allocation_info->voxel_image
        };
        *render_pipeline_info = (region_render_pipeline_info_t) {

        };
    }

    return result_success;
}

void term_region_management(void) {
    for (size_t region_index = 0; region_index < NUM_REGIONS; region_index++) {
        region_allocation_info_t* allocation_info = &region_allocation_infos[region_index];

        vmaDestroyBuffer(allocator, allocation_info->uniform_buffer, allocation_info->uniform_buffer_allocation);
        vkDestroyImageView(device, allocation_info->voxel_image_view, NULL);
        vmaDestroyImage(allocator, allocation_info->voxel_image, allocation_info->voxel_image_allocation);
    }
}