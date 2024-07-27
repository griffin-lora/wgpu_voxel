#include "voxel_generation_compute_pipeline.h"
#include "gfx/gfx.h"
#include "gfx/shader.h"
#include "result.h"
#include "voxel.h"
#include <dawn/webgpu.h>
#include <stdio.h>
#include <string.h>

static WGPUComputePipeline pipeline;
static WGPUBindGroup bind_group;
static WGPUTexture voxel_texture;
WGPUTextureView voxel_texture_view;

result_t init_voxel_generation_compute_pipeline(void) {
    result_t result;

    WGPUShaderModule shader_module;
    if ((result = create_shader_module("shader/voxel_generation.spv", &shader_module)) != result_success) {
        return result;
    }

    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 1,
        .entries = (WGPUBindGroupLayoutEntry[1]) {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Compute,
                .storageTexture = {
                    .access = WGPUStorageTextureAccess_WriteOnly,
                    .format = WGPUTextureFormat_R32Uint,
                    .viewDimension = WGPUTextureViewDimension_3D
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

    if ((voxel_texture = wgpuDeviceCreateTexture(device, &(WGPUTextureDescriptor) {
        .dimension = WGPUTextureDimension_3D,
        .format = WGPUTextureFormat_R32Uint,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .size = { VOXEL_REGION_SIZE, VOXEL_REGION_SIZE, VOXEL_REGION_SIZE },
        .usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding,
        .viewFormatCount = 0,
        .viewFormats = NULL
    })) == NULL) {
        return result_texture_create_failure;
    }

    if ((voxel_texture_view = wgpuTextureCreateView(voxel_texture, &(WGPUTextureViewDescriptor) {
        .aspect = WGPUTextureAspect_All,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_3D,
        .format = WGPUTextureFormat_R32Uint
    })) == NULL) {
        return result_texture_view_create_failure;
    }

    if ((bind_group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor) {
        .layout = bind_group_layout,
        .entryCount = 1,
        .entries = (WGPUBindGroupEntry[1]) {
            {
                .binding = 0,
                .textureView = voxel_texture_view
            }
        }
    })) == NULL) {
        return result_bind_group_create_failure;
    }

    return result_success;
}

result_t run_voxel_generation_compute_pipeline(void) {
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

    return result_success;
}

void term_voxel_generation_compute_pipeline(void) {
    wgpuComputePipelineRelease(pipeline);
    wgpuBindGroupRelease(bind_group);
    wgpuTextureViewRelease(voxel_texture_view);
    wgpuTextureRelease(voxel_texture);
}