#include "voxel_render_pipeline.h"
#include "camera.h"
#include "gfx.h"
#include "gfx/default.h"
#include "gfx/pipeline.h"
#include "gfx/gfx_util.h"
#include "gfx/voxel_meshing_compute_pipeline.h"
#include "util.h"
#include "result.h"
#include "voxel.h"
#include <cglm/types-struct.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define NUM_VOXEL_TEXTURE_LAYERS 4

static const char* voxel_texture_layer_paths[NUM_VOXEL_TEXTURE_LAYERS] = {
    "image/cube_voxel_0.png",
    "image/cube_voxel_1.png",
    "image/cube_voxel_2.png",
    "image/cube_voxel_4.png"
};

static pipeline_t pipeline;
static VkBuffer uniform_buffer;
static VmaAllocation uniform_buffer_allocation;
static VkSampler sampler;
static VkImage image;
static VmaAllocation image_allocation;
static VkImageView image_view;

typedef struct {
    mat4s view_projection;
} voxel_uniform_t;

static voxel_uniform_t* uniforms;
static uint32_t uniform_stride;

result_t init_voxel_render_pipeline(VkCommandBuffer command_buffer, VkFence command_fence, const VkPhysicalDeviceProperties* physical_device_properties) {
    result_t result;

    uint32_t texture_size = 16;
    uint32_t num_mip_levels = ((uint32_t)floorf(log2f((float)max_uint32(texture_size, texture_size)))) + 1;

    const void* pixel_arrays[NUM_VOXEL_TEXTURE_LAYERS];

    for (uint32_t layer_index = 0; layer_index < NUM_VOXEL_TEXTURE_LAYERS; layer_index++) {
        int32_t width;
        int32_t height;

        const void* pixels = stbi_load(voxel_texture_layer_paths[layer_index], &width, &height, (int[1]) { 0 }, STBI_rgb_alpha);

        if (pixels == NULL || width != height || width != (int32_t) texture_size) {
            return result_file_read_failure; // TODO: Use a different error
        }

        pixel_arrays[layer_index] = pixels;
    }

    if ((result = create_image(command_buffer, command_fence, &(VkImageCreateInfo) {
        DEFAULT_VK_SAMPLED_IMAGE,
        .format = VK_FORMAT_R8G8B8_SRGB,
        .extent = { texture_size, texture_size, 1 },
        .mipLevels = num_mip_levels,
        .arrayLayers = NUM_VOXEL_TEXTURE_LAYERS
    }, 4, pixel_arrays, &image, &image_allocation)) != result_success) {
        return result;
    }

    for (uint32_t layer_index = 0; layer_index < NUM_VOXEL_TEXTURE_LAYERS; layer_index++) {
        stbi_image_free((void*) pixel_arrays[layer_index]);
    }

    if (vkCreateImageView(device, &(VkImageViewCreateInfo) {
        DEFAULT_VK_IMAGE_VIEW,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = VK_FORMAT_R8G8B8_SRGB,
        .subresourceRange = {
            .levelCount = num_mip_levels,
            .layerCount = NUM_VOXEL_TEXTURE_LAYERS,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
        }
    }, NULL, &image_view) != VK_SUCCESS) {
        return result_image_view_create_failure;
    }

    if (vkCreateSampler(device, &(VkSamplerCreateInfo) {
        DEFAULT_VK_SAMPLER,
        .maxAnisotropy = physical_device_properties->limits.maxSamplerAnisotropy,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minFilter = VK_FILTER_NEAREST,
        .magFilter = VK_FILTER_NEAREST,
        .anisotropyEnable = VK_FALSE,
        .maxLod = (float)num_mip_levels
    }, NULL, &sampler) != VK_SUCCESS) {
        return result_sampler_create_failure;
    }

    uniform_stride = ceil_to_next_multiple(sizeof(voxel_uniform_t), (uint32_t)physical_device_properties->limits.minUniformBufferOffsetAlignment);

    uint32_t uniforms_num_bytes = sizeof(voxel_uniform_t) * uniform_stride;

    if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
        DEFAULT_VK_UNIFORM_BUFFER,
        .size = uniforms_num_bytes
    }, &staging_allocation_create_info, &uniform_buffer, &uniform_buffer_allocation, NULL) != VK_SUCCESS) {
        return result_buffer_create_failure;
    }

    if (vmaMapMemory(allocator, uniform_buffer_allocation, (void*) &uniforms) != VK_SUCCESS) {
        return result_memory_map_failure;
    }
    
    VkShaderModule vertex_shader_module;
    if ((result = create_shader_module("shader/voxel_vertex.spv", &vertex_shader_module)) != result_success) {
        return result;
    }
    VkShaderModule fragment_shader_module;
    if ((result = create_shader_module("shader/voxel_fragment.spv", &fragment_shader_module)) != result_success) {
        return result;
    }

    if ((result = create_descriptor_set(
        &(VkDescriptorSetLayoutCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            
            .bindingCount = 2,
            .pBindings = (VkDescriptorSetLayoutBinding[2]) {
                {
                    DEFAULT_VK_DESCRIPTOR_BINDING,
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                },
                {
                    DEFAULT_VK_DESCRIPTOR_BINDING,
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                }
            }
        },
        (descriptor_info_t[2]) {
            {
                .type = descriptor_info_type_buffer,
                .buffer = {
                    .buffer = uniform_buffer,
                    .offset = 0,
                    .range = uniforms_num_bytes
                }
            },
            {
                .type = descriptor_info_type_image,
                .image = {
                    .sampler = sampler,
                    .imageView = image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo) {
        DEFAULT_VK_GRAPHICS_PIPELINE,

        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo[2]) {
            {
                DEFAULT_VK_SHADER_STAGE,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertex_shader_module
            },
            {
                DEFAULT_VK_SHADER_STAGE,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment_shader_module
            }
        },

        .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = (VkVertexInputBindingDescription[1]) {
                {
                    .binding = 0,
                    .stride = sizeof(voxel_vertex_t),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
                }
            },

            .vertexAttributeDescriptionCount = 3,
            .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[3]) {
                {
                    .binding = 0,
                    .location = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(voxel_vertex_t, vertex_position)
                },
                {
                    .binding = 0,
                    .location = 1,
                    .format = VK_FORMAT_R32_UINT,
                    .offset = offsetof(voxel_vertex_t, vertex_index)
                },
                {
                    .binding = 0,
                    .location = 2,
                    .format = VK_FORMAT_R32_UINT,
                    .offset = offsetof(voxel_vertex_t, voxel_type)
                }
            }
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) { DEFAULT_VK_RASTERIZATION },
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
            DEFAULT_VK_MULTISAMPLE,
            .rasterizationSamples = render_multisample_flags
        },
        .layout = pipeline.pipeline_layout,
        .renderPass = frame_render_pass
    }, NULL, &pipeline.pipeline) != VK_SUCCESS) {
        return result_graphics_pipelines_create_failure;
    }

    vkDestroyShaderModule(device, vertex_shader_module, NULL);
    vkDestroyShaderModule(device, fragment_shader_module, NULL);

    return result_success;
}

result_t draw_voxel_render_pipeline(VkCommandBuffer command_buffer) {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline_layout, 0, 1, &pipeline.descriptor_set, 0, NULL);

    uniforms[0].view_projection = get_view_projection();

    vkCmdBindVertexBuffers(command_buffer, 0, 1, &voxel_vertex_buffer, (VkDeviceSize[1]) { 0 });
    vkCmdDraw(command_buffer, NUM_CUBE_VOXEL_VERTICES * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE, 1, 0, 0);

    return result_success;
}

void term_voxel_render_pipeline() {
    vmaUnmapMemory(allocator, uniform_buffer_allocation);

    vkDestroyImageView(device, image_view, NULL);
    vmaDestroyImage(allocator, image, image_allocation);
    vkDestroySampler(device, sampler, NULL);
    vmaDestroyBuffer(allocator, uniform_buffer, uniform_buffer_allocation);
    destroy_pipeline(&pipeline);
}