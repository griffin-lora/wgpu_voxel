#include "voxel_meshing_compute_pipeline.h"
#include "gfx/default.h"
#include "gfx/gfx.h"
#include "gfx/gfx_util.h"
#include "gfx/pipeline.h"
#include "gfx/voxel_generation_compute_pipeline.h"
#include "result.h"
#include "voxel.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

static pipeline_t pipeline;
static VkBuffer voxel_vertex_count_buffer;
static VmaAllocation voxel_vertex_count_buffer_allocation;
VkBuffer voxel_vertex_buffer;
static VmaAllocation voxel_vertex_buffer_allocation;

result_t init_voxel_meshing_compute_pipeline(void) {
    result_t result;

    if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
        DEFAULT_VK_BUFFER,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .size = sizeof(uint32_t)
    }, &staging_allocation_create_info, &voxel_vertex_count_buffer, &voxel_vertex_count_buffer_allocation, NULL) != VK_SUCCESS) {
        return result_buffer_create_failure;
    }

    if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
        DEFAULT_VK_BUFFER,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .size = NUM_CUBE_VOXEL_VERTICES * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * sizeof(voxel_vertex_t)
    }, &staging_allocation_create_info, &voxel_vertex_buffer, &voxel_vertex_buffer_allocation, NULL) != VK_SUCCESS) {
        return result_buffer_create_failure;
    }

    VkShaderModule shader_module;
    if ((result = create_shader_module("shader/voxel_meshing.spv", &shader_module)) != result_success) {
        return result;
    }

    if ((result = create_descriptor_set(
        &(VkDescriptorSetLayoutCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            
            .bindingCount = 3,
            .pBindings = (VkDescriptorSetLayoutBinding[3]) {
                {
                    DEFAULT_VK_DESCRIPTOR_BINDING,
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                },
                {
                    DEFAULT_VK_DESCRIPTOR_BINDING,
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                },
                {
                    DEFAULT_VK_DESCRIPTOR_BINDING,
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
                }
            }
        },
        (descriptor_info_t[3]) {
            {
                .type = descriptor_info_type_image,
                .image = {
                    .imageView = voxel_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL
                }
            },
            {
                .type = descriptor_info_type_buffer,
                .buffer = {
                    .buffer = voxel_vertex_count_buffer,
                    .range = sizeof(uint32_t)
                }
            },
            {
                .type = descriptor_info_type_buffer,
                .buffer = {
                    .buffer = voxel_vertex_buffer,
                    .range = NUM_CUBE_VOXEL_VERTICES * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * sizeof(voxel_vertex_t)
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

result_t run_voxel_meshing_compute_pipeline(void) {
    // TODO: Use seperate command buffer from transfer command buffer??

    vkResetFences(device, 1, &transfer_fence);
    vkResetCommandBuffer(transfer_command_buffer, 0);

    if (vkBeginCommandBuffer(transfer_command_buffer, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    }) != VK_SUCCESS) {
        return result_command_buffer_begin_failure;
    }

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

void term_voxel_meshing_compute_pipeline(void) {
    destroy_pipeline(&pipeline);
    vmaDestroyBuffer(allocator, voxel_vertex_count_buffer, voxel_vertex_count_buffer_allocation);
    vmaDestroyBuffer(allocator, voxel_vertex_buffer, voxel_vertex_buffer_allocation);
}