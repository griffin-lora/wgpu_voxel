#include "region_render_pipeline.h"
#include "camera.h"
#include "gfx.h"
#include "gfx/default.h"
#include "gfx/pipeline.h"
#include "gfx/gfx_util.h"
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
VkSampler voxel_sampler;
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
    
    VkShaderModule task_shader_module;
    if ((result = create_shader_module("shader/region_task.spv", &task_shader_module)) != result_success) {
        return result;
    }
    VkShaderModule mesh_shader_module;
    if ((result = create_shader_module("shader/region_mesh.spv", &mesh_shader_module)) != result_success) {
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
        .bindingCount = 2,
        .pBindings = (VkDescriptorSetLayoutBinding[2]) {
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT
            },
            {
                DEFAULT_VK_DESCRIPTOR_BINDING,
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT
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
            .stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT,
            .size = sizeof(push_constants_t)
        }
    }, NULL, &pipeline.pipeline_layout) != VK_SUCCESS) {
        return result_pipeline_layout_create_failure;
    }

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo) {
        DEFAULT_VK_GRAPHICS_PIPELINE,

        .stageCount = 3,
        .pStages = (VkPipelineShaderStageCreateInfo[3]) {
            {
                DEFAULT_VK_SHADER_STAGE,
                .stage = VK_SHADER_STAGE_TASK_BIT_EXT,
                .module = task_shader_module
            },
            {
                DEFAULT_VK_SHADER_STAGE,
                .stage = VK_SHADER_STAGE_MESH_BIT_EXT,
                .module = mesh_shader_module
            },
            {
                DEFAULT_VK_SHADER_STAGE,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment_shader_module
            }
        },

        .pVertexInputState = NULL,
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
    
    vkDestroyShaderModule(device, task_shader_module, NULL);
    vkDestroyShaderModule(device, mesh_shader_module, NULL);
    vkDestroyShaderModule(device, fragment_shader_module, NULL);

    return result_success;
}

result_t draw_region_render_pipeline(VkCommandBuffer command_buffer) {

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    mat4s view_projection = get_view_projection();
    vkCmdPushConstants(command_buffer, pipeline.pipeline_layout, VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(push_constants_t), &view_projection);

    for (uint32_t i = 0; i < NUM_REGIONS; i++) {
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline_layout, 0, 2, (VkDescriptorSet[2]) { region_render_pipeline_infos[i].descriptor_set }, 0, NULL);
        vkCmdDrawMeshTasksEXT(command_buffer, 8, 8, 8);
    }

    return result_success;
}

void term_region_render_pipeline() {
    destroy_pipeline(&pipeline);
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);
    vkDestroyImageView(device, color_image_view, NULL);
    vmaDestroyImage(allocator, color_image, color_image_allocation);
    vkDestroySampler(device, color_sampler, NULL);
    vkDestroySampler(device, voxel_sampler, NULL);
}