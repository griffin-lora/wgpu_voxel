#include "compute_pipeline.h"
#include "gfx/gfx.h"
#include "result.h"

static WGPUComputePipeline pipeline;
static WGPUBindGroup bind_group;

result_t init_compute_pipeline(void) {


    return result_success;
}

result_t run_compute_pipeline(void) {
    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(device, &(WGPUCommandEncoderDescriptor) {});
    if (command_encoder == NULL) {
        return result_command_encoder_create_failure;
    }

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(command_encoder, &(WGPUCommandBufferDescriptor) {});
    if (command == NULL) {
        return result_command_encoder_finish_failure;
    }

    WGPUComputePassEncoder compute_pass_encoder = wgpuCommandEncoderBeginComputePass(command_encoder, &(WGPUComputePassDescriptor) {});
    if (compute_pass_encoder == NULL) {
        return result_compute_pass_encoder_create_failure;
    }

    wgpuComputePassEncoderSetPipeline(compute_pass_encoder, pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, 0, bind_group, 0, NULL);

    wgpuComputePassEncoderEnd(compute_pass_encoder);
    wgpuComputePassEncoderRelease(compute_pass_encoder);

    wgpuQueueSubmit(queue, 1, &command);

    wgpuCommandBufferRelease(command);
    wgpuCommandEncoderRelease(command_encoder);

    return result_success;
}

void term_compute_pipeline(void) {
    // wgpuComputePipelineRelease(pipeline);
    // wgpuBindGroupRelease(bind_group);
}