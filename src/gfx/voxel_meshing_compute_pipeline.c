#include "voxel_meshing_compute_pipeline.h"
#include "gfx/gfx.h"
#include "gfx/shader.h"
#include "gfx/voxel_generation_compute_pipeline.h"
#include "result.h"
#include "voxel.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WGPUComputePipeline pipeline;
static WGPUBindGroup bind_group;
static WGPUBuffer voxel_vertex_count_buffer;
WGPUBuffer voxel_vertex_buffer;
static WGPUQuerySet timestamp_query_set;
static WGPUBuffer timestamp_resolve_buffer;
static WGPUBuffer timestamp_read_buffer;

static void timestamp_buffer_map_callback(WGPUMapAsyncStatus status, const char* msg, void*, void*) {
    if (status != WGPUMapAsyncStatus_Success) {
        printf("Failed to map buffer with message: %s\n", msg);
        exit(1);
    }

    const uint64_t* timestamps = wgpuBufferGetConstMappedRange(timestamp_read_buffer, 0, 2 * sizeof(uint64_t));

    printf("Meshing took: %ld nanoseconds\n", timestamps[1] - timestamps[0]);

    wgpuBufferUnmap(timestamp_read_buffer);
}

result_t init_voxel_meshing_compute_pipeline(void) {
    result_t result;

    WGPUShaderModule shader_module;
    if ((result = create_shader_module("shader/voxel_meshing.spv", &shader_module)) != result_success) {
        return result;
    }

    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 3,
        .entries = (WGPUBindGroupLayoutEntry[3]) {
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
            },
            {
                .binding = 2,
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

    if ((voxel_vertex_count_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .usage = WGPUBufferUsage_Storage,
        .size = sizeof(uint32_t),
        .mappedAtCreation = false
    })) == NULL) {
        return result_buffer_create_failure;
    }

    if ((voxel_vertex_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex,
        .size = NUM_CUBE_VOXEL_VERTICES * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * sizeof(voxel_vertex_t)
    })) == NULL) {
        return result_buffer_create_failure;
    }

    if ((bind_group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor) {
        .layout = bind_group_layout,
        .entryCount = 3,
        .entries = (WGPUBindGroupEntry[3]) {
            {
                .binding = 0,
                .textureView = voxel_texture_view
            },
            {
                .binding = 1,
                .buffer = voxel_vertex_count_buffer,
                .offset = 0,
                .size = sizeof(uint32_t)
            },
            {
                .binding = 2,
                .buffer = voxel_vertex_buffer,
                .offset = 0,
                .size = NUM_CUBE_VOXEL_VERTICES * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * VOXEL_REGION_SIZE * sizeof(voxel_vertex_t)
            }
        }
    })) == NULL) {
        return result_bind_group_create_failure;
    }

    if ((timestamp_query_set = wgpuDeviceCreateQuerySet(device, &(WGPUQuerySetDescriptor) {
        .type = WGPUQueryType_Timestamp,
        .count = 2
    })) == NULL) {
        return result_query_set_create_failure;
    }

    if ((timestamp_resolve_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .size = 2 * sizeof(uint64_t),
        .usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
        .mappedAtCreation = false
    })) == NULL) {
        return result_buffer_create_failure;
    }

    if ((timestamp_read_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .size = 2 * sizeof(uint64_t),
        .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = false
    })) == NULL) {
        return result_buffer_create_failure;
    }

    return result_success;
}

result_t run_voxel_meshing_compute_pipeline(void) {
    wgpuDeviceTick(device); // TODO: Implement proper memory barrier here

    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(device, &(WGPUCommandEncoderDescriptor) {});
    if (command_encoder == NULL) {
        return result_command_encoder_create_failure;
    }

    WGPUComputePassEncoder compute_pass_encoder = wgpuCommandEncoderBeginComputePass(command_encoder, &(WGPUComputePassDescriptor) {
        .timestampWrites = &(WGPUComputePassTimestampWrites) {
            .beginningOfPassWriteIndex = 0,
            .endOfPassWriteIndex = 1,
            .querySet = timestamp_query_set
        }
    });
    if (compute_pass_encoder == NULL) {
        return result_compute_pass_encoder_create_failure;
    }

    wgpuComputePassEncoderSetPipeline(compute_pass_encoder, pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, 0, bind_group, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, 8, 8, 8);

    wgpuComputePassEncoderEnd(compute_pass_encoder);
    wgpuComputePassEncoderRelease(compute_pass_encoder);

    wgpuCommandEncoderResolveQuerySet(command_encoder, timestamp_query_set, 0, 2, timestamp_resolve_buffer, 0);
    wgpuCommandEncoderCopyBufferToBuffer(command_encoder, timestamp_resolve_buffer, 0, timestamp_read_buffer, 0, 2 * sizeof(uint64_t));

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
    wgpuBufferMapAsync2(timestamp_read_buffer, WGPUMapMode_Read, 0, 2 * sizeof(uint64_t), (WGPUBufferMapCallbackInfo2) {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = timestamp_buffer_map_callback
    });

    wgpuComputePipelineRelease(pipeline);
    wgpuBindGroupRelease(bind_group);
    wgpuBufferRelease(voxel_vertex_count_buffer);
    wgpuBufferRelease(voxel_vertex_buffer);
    wgpuQuerySetRelease(timestamp_query_set);
    wgpuBufferRelease(timestamp_resolve_buffer);
    wgpuBufferRelease(timestamp_read_buffer);
}