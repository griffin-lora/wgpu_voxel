#include "gfx/gfx.h"
#include "gfx/pipeline.h"

void destroy_pipeline(const pipeline_t* pipeline) {
    vkDestroyPipeline(device, pipeline->pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline->pipeline_layout, NULL);
    vkDestroyDescriptorPool(device, pipeline->descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(device, pipeline->descriptor_set_layout, NULL);
}