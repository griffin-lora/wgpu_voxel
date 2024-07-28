#pragma once
#include <vulkan/vulkan.h>

typedef struct {
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
} render_pipeline_render_info_t;

void destroy_render_pipeline(const render_pipeline_render_info_t* info);