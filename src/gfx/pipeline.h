#pragma once
#include <vulkan/vulkan.h>

typedef struct {
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
} pipeline_t;

void destroy_pipeline(const pipeline_t* pipeline);