#include "compute_pipeline.h"
#include "gfx/gfx.h"
#include "gfx/shader.h"
#include "result.h"
#include <dawn/webgpu.h>
#include <stdio.h>
#include <string.h>

static WGPUComputePipeline pipeline;
static WGPUBindGroup bind_group;
static WGPUBuffer in_voxel_buffer;
static WGPUBuffer out_voxel_buffer;

typedef struct {
    float x;
} voxel_t;

voxel_t in_voxels[64];
voxel_t out_voxels[64];

result_t init_compute_pipeline(void) {
    result_t result;

    WGPUShaderModule shader_module;
    if ((result = create_shader_module("shader/compute.spv", &shader_module)) != result_success) {
        return result;
    }

    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 2,
        .entries = (WGPUBindGroupLayoutEntry[2]) {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Compute,
                .buffer = {
                    .type = WGPUBufferBindingType_ReadOnlyStorage
                }
            },
            {
                .binding = 1,
                .visibility = WGPUShaderStage_Compute,
                .buffer = {
                    .type = WGPUBufferBindingType_Storage
                }
            }
        }
    });
    if (bind_group_layout == NULL) {
        return result_bind_group_layout_create_failure;
    }

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bind_group_layout
    });
    if (pipeline_layout == NULL) {
        return result_pipeline_layout_create_failure;
    }
    
    if ((pipeline = wgpuDeviceCreateComputePipeline(device, &(WGPUComputePipelineDescriptor) {
        .compute = {
            .entryPoint = "main",
            .module = shader_module
        },
        .layout = pipeline_layout
    })) == NULL) {
        return result_compute_pipeline_create_failure;
    }

    wgpuBindGroupLayoutRelease(bind_group_layout);
    wgpuPipelineLayoutRelease(pipeline_layout);

    for (size_t i = 0; i < 64; i++) {
        in_voxels[i].x = (float) i;
    }

    if ((in_voxel_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .size = sizeof(in_voxels),
        .usage = WGPUBufferUsage_Storage,
        .mappedAtCreation = true
    })) == NULL) {
        return result_buffer_create_failure;
    }

    if ((out_voxel_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .size = sizeof(out_voxels),
        .usage = WGPUBufferUsage_Storage
    })) == NULL) {
        return result_buffer_create_failure;
    }

    memcpy(wgpuBufferGetMappedRange(in_voxel_buffer, 0, sizeof(in_voxels)), in_voxels, sizeof(in_voxels));

    wgpuBufferUnmap(in_voxel_buffer);

    if ((bind_group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor) {
        .layout = bind_group_layout,
        .entryCount = 2,
        .entries = (WGPUBindGroupEntry[2]) {
            {
                .binding = 0,
                .buffer = in_voxel_buffer,
                .offset = 0,
                .size = sizeof(in_voxels)
            },
            {
                .binding = 1,
                .buffer = out_voxel_buffer,
                .offset = 0,
                .size = sizeof(out_voxels)
            }
        }
    })) == NULL) {
        return result_bind_group_create_failure;
    }

    return result_success;
}

result_t run_compute_pipeline(void) {
    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(device, &(WGPUCommandEncoderDescriptor) {});
    if (command_encoder == NULL) {
        return result_command_encoder_create_failure;
    }

    WGPUComputePassEncoder compute_pass_encoder = wgpuCommandEncoderBeginComputePass(command_encoder, &(WGPUComputePassDescriptor) {});
    if (compute_pass_encoder == NULL) {
        return result_compute_pass_encoder_create_failure;
    }

    wgpuComputePassEncoderSetPipeline(compute_pass_encoder, pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, 0, bind_group, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, 1, 1, 1);

    wgpuComputePassEncoderEnd(compute_pass_encoder);
    wgpuComputePassEncoderRelease(compute_pass_encoder);

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(command_encoder, &(WGPUCommandBufferDescriptor) {});
    if (command == NULL) {
        return result_command_encoder_finish_failure;
    }

    wgpuQueueSubmit(queue, 1, &command);

    wgpuCommandBufferRelease(command);
    wgpuCommandEncoderRelease(command_encoder);

    return result_success;
}

void term_compute_pipeline(void) {
    wgpuComputePipelineRelease(pipeline);
    wgpuBindGroupRelease(bind_group);
    wgpuBufferRelease(in_voxel_buffer);
    wgpuBufferRelease(out_voxel_buffer);
}