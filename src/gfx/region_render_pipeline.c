#include "region_render_pipeline.h"
#include "camera.h"
#include "gfx.h"
#include "gfx/default.h"
#include "gfx/pipeline.h"
#include "gfx/gfx_util.h"
#include "gfx/region_meshing_compute_pipeline.h"
#include "util.h"
#include "result.h"
#include "voxel/region_management.h"
#include <cglm/types-struct.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define NUM_COLOR_IMAGE_LAYERS 4

static const char* color_image_layer_paths[NUM_COLOR_IMAGE_LAYERS] = {
    "image/cube_voxel_0.png",
    "image/cube_voxel_1.png",
    "image/cube_voxel_2.png",
    "image/cube_voxel_4.png"
};

static pipeline_t pipeline;
static VkSampler color_sampler;
static VkImage color_image;
static VmaAllocation color_image_allocation;
static VkImageView color_image_view;
static VkDescriptorSetLayout descriptor_set_layout;
static VkDescriptorSet descriptor_set;

typedef struct {
    mat4s view_projection;
} push_constants_t;

VkDescriptorSetLayout region_render_pipeline_set_layout;

result_t init_region_render_pipeline(VkCommandBuffer command_buffer, VkFence command_fence, VkDescriptorPool descriptor_pool, const VkPhysicalDeviceProperties* physical_device_properties) {
    result_t result;

    uint32_t texture_size = 16;
    uint32_t num_mip_levels = ((uint32_t)floorf(log2f((float)max_uint32(texture_size, texture_size)))) + 1;

    const void* pixel_arrays[NUM_COLOR_IMAGE_LAYERS];

    for (uint32_t layer_index = 0; layer_index < NUM_COLOR_IMAGE_LAYERS; layer_index++) {
        int32_t width;
        int32_t height;

        const void* pixels = stbi_load(color_image_layer_paths[layer_index], &width, &height, (int[1]) { 0 }, STBI_rgb_alpha);

        if (pixels == NULL || width != height || width != (int32_t) texture_size) {
            return result_file_read_failure; // TODO: Use a different error
        }

        pixel_arrays[layer_index] = pixels;
    }

    if ((result = create_image(command_buffer, command_fence, &(VkImageCreateInfo) {
        DEFAULT_VK_SAMPLED_IMAGE,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = { texture_size, texture_size, 1 },
        .mipLevels = num_mip_levels,
        .arrayLayers = NUM_COLOR_IMAGE_LAYERS
    }, 4, pixel_arrays, &color_image, &color_image_allocation)) != result_success) {
        return result;
    }

    for (uint32_t layer_index = 0; layer_index < NUM_COLOR_IMAGE_LAYERS; layer_index++) {
        stbi_image_free((void*) pixel_arrays[layer_index]);
    }

    if (vkCreateImageView(device, &(VkImageViewCreateInfo) {
        DEFAULT_VK_IMAGE_VIEW,
        .image = color_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .levelCount = num_mip_levels,
            .layerCount = NUM_COLOR_IMAGE_LAYERS,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
        }
    }, NULL, &color_image_view) != VK_SUCCESS) {
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
    }, NULL, &color_sampler) != VK_SUCCESS) {
        return result_sampler_create_failure;
    }
    
    VkShaderModule vertex_shader_module;
    if ((result = create_shader_module("shader/region_vertex.spv", &vertex_shader_module)) != result_success) {
        return result;
    }
    VkShaderModule fragment_shader_module;
    if ((result = create_shader_module("shader/region_fragment.spv", &fragment_shader_module)) != result_success) {
        return result;
    }

    if (vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = (VkDescriptorSetLayoutBinding[1]) {
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            }
        }
    }, NULL, &descriptor_set_layout) != VK_SUCCESS) {
        return result_descriptor_set_layout_create_failure;
    }

    if (vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout
    }, &descriptor_set) != VK_SUCCESS) {
        return result_descriptor_sets_allocate_failure;
    }
    
    vkUpdateDescriptorSets(device, 1, (VkWriteDescriptorSet[1]) {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .pImageInfo = &(VkDescriptorImageInfo) {
                .sampler = color_sampler,
                .imageView = color_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        }
    }, 0, NULL);

    if (vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = (VkDescriptorSetLayoutBinding[1]) {
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            }
        }
    }, NULL, &region_render_pipeline_set_layout) != VK_SUCCESS) {
        return result_descriptor_set_layout_create_failure;
    }
    
    if (vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo) {
        DEFAULT_VK_PIPELINE_LAYOUT,
        .setLayoutCount = 2,
        .pSetLayouts = (VkDescriptorSetLayout[2]) {
            descriptor_set_layout,
            region_render_pipeline_set_layout
        },
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &(VkPushConstantRange) {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size = sizeof(push_constants_t)
        }
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
                    .stride = sizeof(region_vertex_t),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
                }
            },

            .vertexAttributeDescriptionCount = 3,
            .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[3]) {
                {
                    .binding = 0,
                    .location = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(region_vertex_t, vertex_position)
                },
                {
                    .binding = 0,
                    .location = 1,
                    .format = VK_FORMAT_R32_UINT,
                    .offset = offsetof(region_vertex_t, vertex_index)
                },
                {
                    .binding = 0,
                    .location = 2,
                    .format = VK_FORMAT_R32_UINT,
                    .offset = offsetof(region_vertex_t, voxel_type)
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

result_t draw_region_render_pipeline(VkCommandBuffer command_buffer) {

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    mat4s view_projection = get_view_projection();
    vkCmdPushConstants(command_buffer, pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants_t), &view_projection);

    for (uint32_t i = 0; i < NUM_REGIONS; i++) {
        region_render_pipeline_info_t* info = &region_render_pipeline_infos[i];

        if (info->vertex_buffer == NULL) {
            continue;
        }

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline_layout, 0, 2, (VkDescriptorSet[2]) { descriptor_set, info->descriptor_set }, 0, NULL);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &info->vertex_buffer, (VkDeviceSize[1]) { 0 });

        vkCmdDraw(command_buffer, info->num_vertices, 1, 0, 0);
    }

    return result_success;
}

void term_region_render_pipeline() {
    destroy_pipeline(&pipeline);
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);
    vkDestroyDescriptorSetLayout(device, region_render_pipeline_set_layout, NULL);
    vkDestroyImageView(device, color_image_view, NULL);
    vmaDestroyImage(allocator, color_image, color_image_allocation);
    vkDestroySampler(device, color_sampler, NULL);
}