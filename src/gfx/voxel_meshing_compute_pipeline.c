#include "voxel_meshing_compute_pipeline.h"
#include "gfx/gfx.h"
#include "gfx/shader.h"
#include "gfx/voxel_generation_compute_pipeline.h"
#include "result.h"
#include <dawn/webgpu.h>
#include <stdio.h>
#include <string.h>

static WGPUComputePipeline pipeline;
static WGPUBindGroup bind_group;
WGPUBuffer voxel_vertex_buffer;

result_t init_voxel_meshing_compute_pipeline(void) {
    result_t result;

    WGPUShaderModule shader_module;
    if ((result = create_shader_module("shader/voxel_meshing.spv", &shader_module)) != result_success) {
        return result;
    }

    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 2,
        .entries = (WGPUBindGroupLayoutEntry[2]) {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Compute,
                .texture = {
                    .sampleType = WGPUTextureSampleType_Uint,
                    .viewDimension = WGPUTextureViewDimension_3D
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

    if ((voxel_vertex_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex,
        .size = 32 * 32 * 32 * 6 * sizeof(voxel_vertex_t)
    })) == NULL) {
        return result_buffer_create_failure;
    }

    if ((bind_group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor) {
        .layout = bind_group_layout,
        .entryCount = 2,
        .entries = (WGPUBindGroupEntry[2]) {
            {
                .binding = 0,
                .textureView = voxel_texture_view
            },
            {
                .binding = 1,
                .buffer = voxel_vertex_buffer,
                .offset = 0,
                .size = 32 * 32 * 32 * 6 * sizeof(voxel_vertex_t)
            }
        }
    })) == NULL) {
        return result_bind_group_create_failure;
    }

    return result_success;
}

result_t run_voxel_meshing_compute_pipeline(void) {
    wgpuDeviceTick(device); // TODO: Implement proper memory barrier here

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
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, 8, 8, 8);

    wgpuComputePassEncoderEnd(compute_pass_encoder);
    wgpuComputePassEncoderRelease(compute_pass_encoder);

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(command_encoder, &(WGPUCommandBufferDescriptor) {});
    if (command == NULL) {
        return result_command_encoder_finish_failure;
    }

    wgpuQueueSubmit(queue, 1, &command);

    wgpuCommandBufferRelease(command);
    wgpuCommandEncoderRelease(command_encoder);

    wgpuDeviceTick(device); // TODO: Implement proper memory barrier here

    return result_success;
}

void term_voxel_meshing_compute_pipeline(void) {
    wgpuComputePipelineRelease(pipeline);
    wgpuBindGroupRelease(bind_group);
    wgpuBufferRelease(voxel_vertex_buffer);
}