#include "gfx/gfx.h"
#include "gfx/render_pipeline_info.h"

void destroy_graphics_pipeline(const render_pipeline_render_info_t* info) {
    vkDestroyPipeline(device, info->pipeline, NULL);
    vkDestroyPipelineLayout(device, info->pipeline_layout, NULL);
    vkDestroyDescriptorPool(device, info->descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(device, info->descriptor_set_layout, NULL);
}