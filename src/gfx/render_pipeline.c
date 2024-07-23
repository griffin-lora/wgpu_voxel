#include "render_pipeline.h"
#include "gfx.h"
#include "shader.h"
#include "result.h"
#include <cglm/types-struct.h>
#include <dawn/webgpu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb/stb_image.h>

static WGPURenderPipeline pipeline;
static WGPUBuffer vertex_buffer;
static WGPUBuffer index_buffer;
static WGPUBuffer uniform_buffer;
static WGPUBindGroup bind_group;
static WGPUSampler sampler;
static WGPUTexture texture;
static WGPUTextureView texture_view;

typedef struct {
    vec2s position;
    vec2s texel_coord;
} vertex_t;

static vertex_t vertices[4];
static uint16_t indices[6] = {
    0, 1, 2,
    0, 2, 3
};

typedef struct {
    vec2s offset;
} uniform_t;

static uniform_t uniforms[2];

static uint32_t ceil_to_next_multiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

result_t init_render_pipeline(void) {
    result_t result;

    vertices[0] = (vertex_t) {
        (vec2s) {{ -0.5f, -0.5f }},
        (vec2s) {{ 0.0f, 1.0f }}
    };
    vertices[1] = (vertex_t) {
        (vec2s) {{ 0.5f, -0.5f }},
        (vec2s) {{ 1.0f, 1.0f }}
    };
    vertices[2] = (vertex_t) {
        (vec2s) {{ 0.5f, 0.5f }},
        (vec2s) {{ 1.0f, 0.0f }}
    };
    vertices[3] = (vertex_t) {
        (vec2s) {{ -0.5f, 0.5f }},
        (vec2s) {{ 0.0f, 0.0f }}
    };

    vertex_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .usage = WGPUBufferUsage_Vertex,
        .size = sizeof(vertices),
        .mappedAtCreation = true
    });

    index_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .usage = WGPUBufferUsage_Index,
        .size = sizeof(indices),
        .mappedAtCreation = true
    });

    uint32_t uniform_stride = ceil_to_next_multiple(sizeof(*uniforms), device_limits.limits.minUniformBufferOffsetAlignment);

    uint32_t uniforms_num_bytes = (sizeof(uniforms) / sizeof(*uniforms)) * uniform_stride;

    uniform_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .size = uniforms_num_bytes,
        .mappedAtCreation = false
    });
    
    if (vertex_buffer == NULL || index_buffer == NULL || uniform_buffer == NULL) {
        return result_buffer_create_failure;
    }

    memcpy(wgpuBufferGetMappedRange(vertex_buffer, 0, sizeof(vertices)), vertices, sizeof(vertices));
    memcpy(wgpuBufferGetMappedRange(index_buffer, 0, sizeof(indices)), indices, sizeof(indices));

    wgpuBufferUnmap(vertex_buffer);
    wgpuBufferUnmap(index_buffer);

    WGPUShaderModule vertex_shader_module;
    WGPUShaderModule fragment_shader_module;
    if ((result = create_shader_module("shader/vertex.spv", &vertex_shader_module)) != result_success) {
        return result;
    }
    if ((result = create_shader_module("shader/fragment.spv", &fragment_shader_module)) != result_success) {
        return result;
    }

    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 3,
        .entries = (WGPUBindGroupLayoutEntry[3]) {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Vertex,
                .buffer = {
                    .type = WGPUBufferBindingType_Uniform,
                    .minBindingSize = sizeof(*uniforms),
                    .hasDynamicOffset = true
                }
            },
            {
                .binding = 1,
                .visibility = WGPUShaderStage_Fragment,
                .texture = {
                    .sampleType = WGPUTextureSampleType_Float,
                    .viewDimension = WGPUTextureViewDimension_2D
                }
            },
            {
                .binding = 2,
                .visibility = WGPUShaderStage_Fragment,
                .sampler = {
                    .type = WGPUSamplerBindingType_Filtering
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

    if ((pipeline = wgpuDeviceCreateRenderPipeline(device, &(WGPURenderPipelineDescriptor) {
        .vertex = {
            .bufferCount = 1,
            .buffers = &(WGPUVertexBufferLayout) {
                .attributeCount = 2,
                .attributes = (WGPUVertexAttribute[2]) {
                    {
                        .format = WGPUVertexFormat_Float32x2,
                        .offset = 0,
                        .shaderLocation = 0
                    },
                    {
                        .format = WGPUVertexFormat_Float32x2,
                        .offset = sizeof(vertices->position),
                        .shaderLocation = 1
                    }
                },
                .arrayStride = sizeof(*vertices),
                .stepMode = WGPUVertexStepMode_Vertex
            },
            .module = vertex_shader_module,
            .entryPoint = "main",
            .constantCount = 0,
            .constants = NULL
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None // TODO: Update this
        },
        .fragment = &(WGPUFragmentState) {
            .module = fragment_shader_module,
            .entryPoint = "main",
            .constantCount = 0,
            .constants = NULL,
            .targetCount = 1,
            .targets = &(WGPUColorTargetState) {
                .format = surface_format,
                .blend = &(WGPUBlendState) {
                    .color = {
                        .srcFactor = WGPUBlendFactor_SrcAlpha,
                        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                        .operation = WGPUBlendOperation_Add
                    },
                    .alpha = {
                        .srcFactor = WGPUBlendFactor_Zero,
                        .dstFactor = WGPUBlendFactor_One,
                        .operation = WGPUBlendOperation_Add
                    }
                },
                .writeMask = WGPUColorWriteMask_All
            }
        },
        .multisample = {
            .count = 1,
            .mask = ~0u,
            .alphaToCoverageEnabled = false
        },
        .layout = pipeline_layout,
        .depthStencil = &(WGPUDepthStencilState) {
            .depthCompare = WGPUCompareFunction_Less,
            .depthWriteEnabled = true,
            .format = WGPUTextureFormat_Depth24Plus,
            .stencilFront = {
                .compare = WGPUCompareFunction_Always,
                .failOp = WGPUStencilOperation_Keep,
                .depthFailOp = WGPUStencilOperation_Keep,
                .passOp = WGPUStencilOperation_Keep
            },
            .stencilBack = {
                .compare = WGPUCompareFunction_Always,
                .failOp = WGPUStencilOperation_Keep,
                .depthFailOp = WGPUStencilOperation_Keep,
                .passOp = WGPUStencilOperation_Keep
            }
        }
    })) == NULL) {
        return result_render_pipeline_create_failure;
    }

    wgpuShaderModuleRelease(vertex_shader_module);
    wgpuShaderModuleRelease(fragment_shader_module);

    wgpuBindGroupLayoutRelease(bind_group_layout);
    wgpuPipelineLayoutRelease(pipeline_layout);

    if ((sampler = wgpuDeviceCreateSampler(device, &(WGPUSamplerDescriptor) {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 1
    })) == NULL) {
        return result_sampler_create_failure;
    }

    if ((texture = wgpuDeviceCreateTexture(device, &(WGPUTextureDescriptor) {
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .size = { 2048, 2048, 1 },
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .viewFormatCount = 0,
        .viewFormats = NULL
    })) == NULL) {
        return result_texture_create_failure;
    }

    if ((texture_view = wgpuTextureCreateView(texture, &(WGPUTextureViewDescriptor) {
        .aspect = WGPUTextureAspect_All,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = WGPUTextureFormat_RGBA8Unorm
    })) == NULL) {
        return result_texture_view_create_failure;
    }

    int dummy;

    const void* texture_pixels = stbi_load("texture/test.jpg", &dummy, &dummy, (int[1]) { 0 }, STBI_rgb_alpha);

    if (texture_pixels == NULL) {
        return result_file_read_failure; // TODO: Use a different error
    }

    wgpuQueueWriteTexture(queue, &(WGPUImageCopyTexture) {
        .texture = texture,
        .mipLevel = 0,
        .origin = { 0, 0, 0 },
        .aspect = WGPUTextureAspect_All
    }, texture_pixels, 2048 * 2048 * 4, &(WGPUTextureDataLayout) {
        .offset = 0,
        .bytesPerRow = 4 * 2048,
        .rowsPerImage = 2048
    }, &(WGPUExtent3D) {
        .width = 2048,
        .height = 2048,
        .depthOrArrayLayers = 1
    });

    stbi_image_free((void*) texture_pixels);

    if ((bind_group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor) {
        .layout = bind_group_layout,
        .entryCount = 3,
        .entries = (WGPUBindGroupEntry[3]) {
            {
                .binding = 0,
                .buffer = uniform_buffer,
                .offset = 0,
                .size = sizeof(*uniforms)
            },
            {
                .binding = 1,
                .textureView = texture_view
            },
            {
                .binding = 2,
                .sampler = sampler
            }
        }
    })) == NULL) {
        return result_bind_group_create_failure;
    }

    return result_success;
}

static float time = 0.0f;

result_t draw_render_pipeline(WGPUCommandEncoder command_encoder, WGPUTextureView surface_texture_view, WGPUTextureView depth_texture_view) {
    uint32_t uniform_stride = ceil_to_next_multiple(sizeof(*uniforms), device_limits.limits.minUniformBufferOffsetAlignment);

    time += 0.01f;

    uniforms[0].offset = (vec2s) {{ 0.3f * cosf(time) - 0.5f, 0.3f * sinf(time) }};
    uniforms[1].offset = (vec2s) {{ 0.3f * cosf(time) + 0.5f, 0.3f * sinf(time) }};

    for (size_t i = 0; i < 2; i++) {
        wgpuQueueWriteBuffer(queue, uniform_buffer, uniform_stride * i, &uniforms[i], sizeof(uniforms[i]));
    }

    WGPURenderPassEncoder render_pass_encoder = wgpuCommandEncoderBeginRenderPass(command_encoder, &(WGPURenderPassDescriptor) {
        .colorAttachmentCount = 1,
        .colorAttachments = &(WGPURenderPassColorAttachment) {
            .view = surface_texture_view,
            .resolveTarget = NULL,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = (WGPUColor) { 0.9, 0.1, 0.2, 1.0 },
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
        },
        .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment) {
            .view = depth_texture_view,
            .depthClearValue = 1.0f,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .stencilClearValue = 0,
            .stencilLoadOp = WGPULoadOp_Undefined,
            .stencilStoreOp = WGPUStoreOp_Undefined,
            .stencilReadOnly = true
        },
        .timestampWrites = NULL // TODO: Implement later
    });

    if (render_pass_encoder == NULL) {
        return result_render_pass_encoder_create_failure;
    }
    
    wgpuRenderPassEncoderSetPipeline(render_pass_encoder, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, vertex_buffer, 0, sizeof(vertices));
    wgpuRenderPassEncoderSetIndexBuffer(render_pass_encoder, index_buffer, WGPUIndexFormat_Uint16, 0, sizeof(indices));

    for (size_t i = 0; i < 2; i++) {
        wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, bind_group, 1, (uint32_t[1]) { uniform_stride * (uint32_t) i });
        wgpuRenderPassEncoderDrawIndexed(render_pass_encoder, sizeof(indices) / sizeof(*indices), 1, 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(render_pass_encoder);
    wgpuRenderPassEncoderRelease(render_pass_encoder);

    return result_success;
}

void term_render_pipeline() {
    wgpuTextureViewRelease(texture_view);
    wgpuTextureRelease(texture);
    wgpuSamplerRelease(sampler);
    wgpuBindGroupRelease(bind_group);
    wgpuBufferRelease(vertex_buffer);
    wgpuBufferRelease(index_buffer);
    wgpuBufferRelease(uniform_buffer);
    wgpuRenderPipelineRelease(pipeline);
}