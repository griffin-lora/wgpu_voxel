#include "voxel_generation_compute_pipeline.h"
#include "gfx/default.h"
#include "gfx/gfx.h"
#include "gfx/gfx_util.h"
#include "gfx/pipeline.h"
#include "result.h"
#include "voxel.h"
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

static pipeline_t pipeline;
static VkImage voxel_image;
static VmaAllocation voxel_image_allocation;
VkImageView voxel_image_view;

result_t init_voxel_generation_compute_pipeline(void) {
    result_t result;

    if (vmaCreateImage(allocator, &(VkImageCreateInfo) {
        DEFAULT_VK_IMAGE,
        .imageType = VK_IMAGE_TYPE_3D,
        .format = VK_FORMAT_R32_UINT,
        .extent = { VOXEL_REGION_SIZE, VOXEL_REGION_SIZE, VOXEL_REGION_SIZE },
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    }, &device_allocation_create_info, &voxel_image, &voxel_image_allocation, NULL) != VK_SUCCESS) {
        return result_image_create_failure;
    }

    if (vkCreateImageView(device, &(VkImageViewCreateInfo) {
        DEFAULT_VK_IMAGE_VIEW,
        .image = voxel_image,
        .viewType = VK_IMAGE_VIEW_TYPE_3D,
        .format = VK_FORMAT_R32_UINT,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
    }, NULL, &voxel_image_view) != VK_SUCCESS) {
        return result_image_view_create_failure;
    }

    VkShaderModule shader_module;
    if ((result = create_shader_module("shader/voxel_generation.spv", &shader_module)) != result_success) {
        return result;
    }
    if ((result = create_descriptor_set(
        &(VkDescriptorSetLayoutCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            
            .bindingCount = 1,
            .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                {
                    DEFAULT_VK_DESCRIPTOR_BINDING,
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                }
            }
        },
        (descriptor_info_t[1]) {
            {
                .type = descriptor_info_type_image,
                .image = {
                    .imageView = voxel_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL
                }
            }
        },
        &pipeline.descriptor_set_layout, &pipeline.descriptor_pool, &pipeline.descriptor_set
    )) != result_success) {
        return result;
    }

    if (vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo) {
        DEFAULT_VK_PIPELINE_LAYOUT,
        .pSetLayouts = &pipeline.descriptor_set_layout
    }, NULL, &pipeline.pipeline_layout) != VK_SUCCESS) {
        return result_pipeline_layout_create_failure;
    }

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &(VkComputePipelineCreateInfo) {
        DEFAULT_VK_COMPUTE_PIPELINE,
        .stage = {
            DEFAULT_VK_SHADER_STAGE,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module
        },
        .layout = pipeline.pipeline_layout
    }, NULL, &pipeline.pipeline) != VK_SUCCESS) {
        return result_compute_pipelines_create_failure;
    }

    vkDestroyShaderModule(device, shader_module, NULL);

    return result_success;
}

result_t run_voxel_generation_compute_pipeline(void) {
    // TODO: Use seperate command buffer from transfer command buffer??

    vkResetFences(device, 1, &transfer_fence);
    vkResetCommandBuffer(transfer_command_buffer, 0);

    if (vkBeginCommandBuffer(transfer_command_buffer, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    }) != VK_SUCCESS) {
        return result_command_buffer_begin_failure;
    }

    vkCmdPipelineBarrier(transfer_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier) {
        DEFAULT_VK_IMAGE_MEMORY_BARRIER,
        .image = voxel_image,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    });
    vkCmdBindPipeline(transfer_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);
    vkCmdBindDescriptorSets(transfer_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline_layout, 0, 1, &pipeline.descriptor_set, 0, NULL);

    vkCmdDispatch(transfer_command_buffer, 8, 8, 8);

    if (vkEndCommandBuffer(transfer_command_buffer) != VK_SUCCESS) {
        return result_command_buffer_end_failure;
    }

    vkQueueSubmit(queue, 1, &(VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transfer_command_buffer
    }, transfer_fence);

    vkWaitForFences(device, 1, &transfer_fence, VK_TRUE, UINT64_MAX);

    return result_success;
}

void term_voxel_generation_compute_pipeline(void) {
    destroy_pipeline(&pipeline);
    vkDestroyImageView(device, voxel_image_view, NULL);
    vmaDestroyImage(allocator, voxel_image, voxel_image_allocation);
}