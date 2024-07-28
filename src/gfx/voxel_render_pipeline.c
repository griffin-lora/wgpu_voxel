#include "voxel_render_pipeline.h"
#include "camera.h"
#include "gfx.h"
#include "gfx/default.h"
#include "gfx/pipeline.h"
#include "gfx/gfx_util.h"
#include "gfx/voxel_meshing_compute_pipeline.h"
#include "util.h"
#include "result.h"
#include <cglm/types-struct.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>

#define NUM_VOXEL_TEXTURE_IMAGES 1
#define NUM_VOXEL_TEXTURE_LAYERS 4

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

VkBuffer voxel_vertex_buffer;
static VmaAllocation voxel_vertex_buffer_allocation;

result_t init_voxel_render_pipeline(const VkPhysicalDeviceProperties* physical_device_properties) {
    result_t result;

    struct {
        const char* path;
        int channels;
    } image_load_infos[NUM_VOXEL_TEXTURE_IMAGES][NUM_VOXEL_TEXTURE_LAYERS] = {
        {
            { "image/cube_voxel_0.png", STBI_rgb },
            { "image/cube_voxel_1.png", STBI_rgb },
            { "image/cube_voxel_2.png", STBI_rgb },
            { "image/cube_voxel_4.png", STBI_rgb }
        }
    };

    void* pixel_arrays[NUM_VOXEL_TEXTURE_IMAGES][NUM_VOXEL_TEXTURE_LAYERS];

    image_create_info_t image_create_infos[NUM_VOXEL_TEXTURE_IMAGES] = {
        {
            .num_pixel_bytes = 3,
            .info = {
                DEFAULT_VK_SAMPLED_IMAGE,
                .format = VK_FORMAT_R8G8B8_SRGB,
                .arrayLayers = NUM_VOXEL_TEXTURE_LAYERS
            }
        }
    };

    for (size_t i = 0; i < NUM_VOXEL_TEXTURE_IMAGES; i++) {
        uint32_t width;
        uint32_t height;

        image_create_info_t* info = &image_create_infos[i];
        info->pixel_arrays = (void**)&pixel_arrays[i];
        
        for (size_t j = 0; j < info->info.arrayLayers; j++) {
            int new_width;
            int new_height;
            info->pixel_arrays[j] = stbi_load(image_load_infos[i][j].path, &new_width, &new_height, (int[1]) { 0 }, image_load_infos[i][j].channels);

            if (info->pixel_arrays[j] == NULL) {
                return result_image_pixels_load_failure;
            }

            if (j > 0 && ((uint32_t)new_width != width || (uint32_t)new_width != height)) {
                return result_image_dimensions_invalid;
            }

            width = (uint32_t)new_width;
            height = (uint32_t)new_height;

            info->info.extent.width = width;
            info->info.extent.height = height;
            info->info.mipLevels = ((uint32_t)floorf(log2f((float)max_uint32(width, height)))) + 1;
        }
    }

    staging_t image_stagings[NUM_VOXEL_TEXTURE_IMAGES];

    if ((result = begin_images(NUM_VOXEL_TEXTURE_IMAGES, image_create_infos, image_stagings, &image, &image_allocation)) != result_success) {
        return result;
    }

    for (size_t i = 0; i < NUM_VOXEL_TEXTURE_IMAGES; i++) {
        const image_create_info_t* info = &image_create_infos[i];
        
        for (size_t j = 0; j < info->info.arrayLayers; j++) {
            stbi_image_free(info->pixel_arrays[j]);
        }
    }

    if (vkCreateSampler(device, &(VkSamplerCreateInfo) {
        DEFAULT_VK_SAMPLER,
        .maxAnisotropy = physical_device_properties->limits.maxSamplerAnisotropy,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minFilter = VK_FILTER_NEAREST,
        .magFilter = VK_FILTER_NEAREST,
        .anisotropyEnable = VK_FALSE,
        .maxLod = (float)image_create_infos[0].info.mipLevels
    }, NULL, &sampler) != VK_SUCCESS) {
        return result_sampler_create_failure;
    }

    vkResetFences(device, 1, &transfer_fence);
    vkResetCommandBuffer(transfer_command_buffer, 0);

    if (vkBeginCommandBuffer(transfer_command_buffer, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    }) != VK_SUCCESS) {
        return result_command_buffer_begin_failure;
    }

    transfer_images(transfer_command_buffer, NUM_VOXEL_TEXTURE_IMAGES, image_create_infos, image_stagings, &image);

    if (vkEndCommandBuffer(transfer_command_buffer) != VK_SUCCESS) {
        return result_command_buffer_end_failure;
    }

    vkQueueSubmit(queue, 1, &(VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transfer_command_buffer
    }, transfer_fence);

    vkWaitForFences(device, 1, &transfer_fence, VK_TRUE, UINT64_MAX);

    end_images(NUM_VOXEL_TEXTURE_IMAGES, image_stagings);

    //

    for (size_t i = 0; i < NUM_VOXEL_TEXTURE_IMAGES; i++) {
        const VkImageCreateInfo* image_create_info = &image_create_infos[i].info;

        if (vkCreateImageView(device, &(VkImageViewCreateInfo) {
            DEFAULT_VK_IMAGE_VIEW,
            .image = image,
            .viewType = image_create_info->arrayLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = image_create_info->format,
            .subresourceRange.levelCount = image_create_info->mipLevels,
            .subresourceRange.layerCount = image_create_info->arrayLayers,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
        }, NULL, &image_view) != VK_SUCCESS) {
            return result_image_view_create_failure;
        }
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

    vec3s vertex_position = (vec3s) {{ 0.0f, 0.0f, 0.0f }};

    voxel_vertex_t vertices[6] = {
        { vertex_position, 0, 0 },
        { vertex_position, 1, 0 },
        { vertex_position, 2, 0 },
        { vertex_position, 3, 0 },
        { vertex_position, 4, 0 },
        { vertex_position, 5, 0 }
    };

    if (vmaCreateBuffer(allocator, &(VkBufferCreateInfo) {
        DEFAULT_VK_VERTEX_BUFFER,
        .size = sizeof(vertices)
    }, &staging_allocation_create_info, &voxel_vertex_buffer, &voxel_vertex_buffer_allocation, NULL) != VK_SUCCESS) {
        return result_buffer_create_failure;
    }

    write_to_buffer(voxel_vertex_buffer_allocation, sizeof(vertices), vertices);
    
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

    // vkCmdBindVertexBuffers(command_buffer, 0, 1, &voxel_vertex_buffer, (VkDeviceSize[1]) { 0 });
    // vkCmdDraw(command_buffer, NUM_CUBE_VOXEL_VERTICES * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE, 1, 0, 0);

    vkCmdBindVertexBuffers(command_buffer, 0, 1, &voxel_vertex_buffer, (VkDeviceSize[1]) { 0 });
    vkCmdDraw(command_buffer, 12, 1, 0, 0);
    
    return result_success;
}

void term_voxel_render_pipeline() {
    vmaUnmapMemory(allocator, uniform_buffer_allocation);

    vkDestroyImageView(device, image_view, NULL);
    vmaDestroyImage(allocator, image, image_allocation);
    vkDestroySampler(device, sampler, NULL);
    vmaDestroyBuffer(allocator, uniform_buffer, uniform_buffer_allocation);
    vmaDestroyBuffer(allocator, voxel_vertex_buffer, voxel_vertex_buffer_allocation);
    destroy_pipeline(&pipeline);
}