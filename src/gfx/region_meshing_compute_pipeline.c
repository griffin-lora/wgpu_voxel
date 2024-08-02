#include "region_meshing_compute_pipeline.h"
#include "gfx/default.h"
#include "gfx/gfx.h"
#include "gfx/gfx_util.h"
#include "gfx/pipeline.h"
#include "result.h"
#include "util.h"
#include "voxel/region.h"
#include "voxel/region_management.h"
#include "voxel/voxel.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define NUM_STAGINGS 8

static pipeline_t pipeline;
static VkDescriptorSetLayout descriptor_set_layout;

static VkBuffer vertex_count_buffer;
static VmaAllocation vertex_count_buffer_allocation;

static VkBuffer vertex_staging_buffer;
static VmaAllocation vertex_staging_buffer_allocation;

static VkDescriptorSet staging_descriptor_sets[NUM_STAGINGS];

VkDescriptorSetLayout region_meshing_compute_pipeline_set_layout;
VkSampler voxel_sampler;

static VkDescriptorPool descriptor_pool;

static size_t vertex_count_stride;
static size_t vertex_staging_stride;

result_t init_region_meshing_compute_pipeline(const VkPhysicalDeviceProperties* physical_device_properties) {
    result_t result;

    vertex_count_stride = ceil_to_next_multiple(sizeof(uint32_t), (uint32_t) physical_device_properties->limits.minStorageBufferOffsetAlignment);
    vertex_staging_stride = ceil_to_next_multiple(NUM_CUBE_VOXEL_VERTICES * REGION_SIZE * REGION_SIZE * REGION_SIZE * sizeof(region_vertex_t), (uint32_t) physical_device_properties->limits.minStorageBufferOffsetAlignment);

    if (vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = (VkDescriptorPoolSize[1]) {
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1
            }
        },
        .maxSets = NUM_STAGINGS
    }, NULL, &descriptor_pool) != VK_SUCCESS) {
        return result_descriptor_pool_create_failure;
    }

    if (vkCreateSampler(device, &(VkSamplerCreateInfo) {
        DEFAULT_VK_SAMPLER,
        .maxAnisotropy = physical_device_properties->limits.maxSamplerAnisotropy,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .unnormalizedCoordinates = VK_TRUE,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .magFilter = VK_FILTER_NEAREST,
        .anisotropyEnable = VK_FALSE,
        .maxLod = 0.0f
    }, NULL, &voxel_sampler) != VK_SUCCESS) {
        return result_sampler_create_failure;
    }

    // TODO: Stop using unperformant shared read for this buffer, instead create a device side buffer and transfer it over for reading at the end of the compute pipeline
    if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
        DEFAULT_VK_BUFFER,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .size = NUM_STAGINGS * vertex_count_stride
    }, &shared_read_allocation_create_info, &vertex_count_buffer, &vertex_count_buffer_allocation, NULL) != VK_SUCCESS) {
        return result_buffer_create_failure;
    }

    if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
        DEFAULT_VK_BUFFER,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .size = NUM_STAGINGS * vertex_staging_stride
    }, &device_allocation_create_info, &vertex_staging_buffer, &vertex_staging_buffer_allocation, NULL) != VK_SUCCESS) {
        return result_buffer_create_failure;
    }

    VkShaderModule shader_module;
    if ((result = create_shader_module("shader/region_meshing.spv", &shader_module)) != result_success) {
        return result;
    }

    if (vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = (VkDescriptorSetLayoutBinding[2]) {
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
            },
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
            }
        }
    }, NULL, &descriptor_set_layout) != VK_SUCCESS) {
        return result_descriptor_set_layout_create_failure;
    }

    for (size_t i = 0; i < NUM_STAGINGS; i++) {
        if (vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptor_set_layout
        }, &staging_descriptor_sets[i]) != VK_SUCCESS) {
            return result_descriptor_sets_allocate_failure;
        }

        vkUpdateDescriptorSets(device, 2, (VkWriteDescriptorSet[2]) {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = staging_descriptor_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = vertex_count_buffer,
                    .offset = vertex_count_stride * i,
                    .range = vertex_count_stride
                }
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = staging_descriptor_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo) {
                    .buffer = vertex_staging_buffer,
                    .offset = vertex_staging_stride * i,
                    .range = vertex_staging_stride
                }
            }
        }, 0, NULL);
    }

    if (vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = (VkDescriptorSetLayoutBinding[1]) {
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
            }
        }
    }, NULL, &region_meshing_compute_pipeline_set_layout) != VK_SUCCESS) {
        return result_descriptor_set_layout_create_failure;
    }

    if (vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo) {
        DEFAULT_VK_PIPELINE_LAYOUT,
        .setLayoutCount = 2,
        .pSetLayouts = (VkDescriptorSetLayout[2]) {
            descriptor_set_layout,
            region_meshing_compute_pipeline_set_layout
        },
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

result_t record_region_meshing_compute_pipeline(VkCommandBuffer command_buffer) {
    if (vkBeginCommandBuffer(command_buffer, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    }) != VK_SUCCESS) {
        return result_command_buffer_begin_failure;
    }

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);
    
    size_t staging_index = 0;
    for (size_t region_index = 0; region_index < NUM_REGIONS; region_index++) {
        if (staging_index == NUM_STAGINGS) {
            break;
        }

        if (region_mesh_states[region_index] != region_mesh_state_await_meshing_compute) {
            continue;
        }
        region_mesh_states[region_index] = region_mesh_state_await_vertex_buffer_creation;
        
        // Clear the vertex count buffer since it is reused
        vkCmdFillBuffer(command_buffer, vertex_count_buffer, vertex_count_stride * staging_index, vertex_count_stride, 0);
        
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline_layout, 0, 2, (VkDescriptorSet[2]) { staging_descriptor_sets[staging_index], region_meshing_compute_pipeline_infos[region_index].descriptor_set }, 0, NULL);

        vkCmdDispatch(command_buffer, 8, 8, 8);

        staging_index++;
    }

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        return result_command_buffer_end_failure;
    }

    return result_success;
}

result_t create_vertex_buffers_for_awaiting_regions(VkCommandBuffer command_buffer, VkFence command_fence) {
    result_t result;

    uint32_t num_vertices_array[NUM_STAGINGS];
    {
        const uint8_t* vertex_count_buffer_mapped;
        if (vmaMapMemory(allocator, vertex_count_buffer_allocation, (void**) &vertex_count_buffer_mapped) != VK_SUCCESS) {
            return result_memory_map_failure;
        }

        for (size_t i = 0; i < NUM_STAGINGS; i++) {
            num_vertices_array[i] = *(const uint32_t*) &vertex_count_buffer_mapped[vertex_count_stride * i];
        }

        vmaUnmapMemory(allocator, vertex_count_buffer_allocation);
    }

    for (size_t i = 0; i < NUM_STAGINGS; i++) {
        printf("Voxel mesh vertices %ld: %d\n", i, num_vertices_array[i]);
    }

    if (vkBeginCommandBuffer(command_buffer, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    }) != VK_SUCCESS) {
        return result_command_buffer_begin_failure;
    }

    size_t staging_index = 0;
    for (size_t region_index = 0; region_index < NUM_REGIONS; region_index++) {
        if (staging_index == NUM_STAGINGS) {
            break;
        }

        if (region_mesh_states[region_index] != region_mesh_state_await_vertex_buffer_creation) {
            continue;
        }
        region_mesh_states[region_index] = region_mesh_state_completed;
        
        uint32_t num_vertices = num_vertices_array[staging_index];

        region_allocation_info_t* allocation_info = &region_allocation_infos[region_index];
        
        if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
            DEFAULT_VK_VERTEX_BUFFER,
            .size = num_vertices * sizeof(region_vertex_t)
        }, &device_allocation_create_info, &allocation_info->vertex_buffer, &allocation_info->vertex_buffer_allocation, NULL) != VK_SUCCESS) {
            return result_buffer_create_failure;
        }

        vkCmdCopyBuffer(command_buffer, vertex_staging_buffer, allocation_info->vertex_buffer, 1, &(VkBufferCopy) {
            .srcOffset = NUM_CUBE_VOXEL_VERTICES * REGION_SIZE * REGION_SIZE * REGION_SIZE * sizeof(region_vertex_t) * staging_index,
            .size = num_vertices * sizeof(region_vertex_t)
        });

        region_render_pipeline_infos[region_index].vertex_buffer = allocation_info->vertex_buffer;

        region_render_pipeline_info_t* render_pipeline_info = &region_render_pipeline_infos[region_index];
        render_pipeline_info->num_vertices = num_vertices;
        render_pipeline_info->vertex_buffer = allocation_info->vertex_buffer;

        staging_index++;
    }

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        return result_command_buffer_end_failure;
    }

    if ((result = submit_and_wait(command_buffer, command_fence)) != result_success) {
        return result;
    }
    if ((result = reset_command_processing(command_buffer, command_fence)) != result_success) {
        return result;
    }

    return result_success;
}

void term_region_meshing_compute_pipeline(void) {
    destroy_pipeline(&pipeline);
    vmaDestroyBuffer(allocator, vertex_count_buffer, vertex_count_buffer_allocation);
    vmaDestroyBuffer(allocator, vertex_staging_buffer, vertex_staging_buffer_allocation);

    vkDestroyDescriptorSetLayout(device, region_meshing_compute_pipeline_set_layout, NULL);
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);
    vkDestroySampler(device, voxel_sampler, NULL);
    vkDestroyDescriptorPool(device, descriptor_pool, NULL);
}