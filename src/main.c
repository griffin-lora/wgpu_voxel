#include "result.h"
#include <GLFW/glfw3native.h>
#include <cglm/types-struct.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <dawn/webgpu.h>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <cglm/struct/vec2.h>
#include <stb/stb_image.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

static GLFWwindow* window;
static WGPUInstance instance;
static WGPUSurface surface;
static WGPUAdapter adapter;
static WGPUDevice device;
static WGPUQueue queue;
static WGPURenderPipeline pipeline;
static WGPUBuffer vertex_buffer;
static WGPUBuffer index_buffer;
static WGPUBuffer uniform_buffer;
static WGPUBindGroup bind_group;
static WGPUSupportedLimits device_limits;
static WGPUTexture depth_texture;
static WGPUTextureView depth_texture_view;
static WGPUSampler sampler;
static WGPUTexture texture;
static WGPUTextureView texture_view;

typedef struct {
    vec2s position;
    vec2s tex_coord;
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

#define MAKE_REQUEST_CALLBACK(TYPE, VAR) \
static void request_##VAR(WGPURequest##TYPE##Status, WGPU##TYPE local_##VAR, char const*, void*) { \
    VAR = local_##VAR; \
}

MAKE_REQUEST_CALLBACK(Adapter, adapter)
MAKE_REQUEST_CALLBACK(Device, device)

static void device_lost_callback(const WGPUDevice*, WGPUDeviceLostReason reason, const char* msg, void*) {
    if (reason == WGPUDeviceLostReason_Destroyed) {
        return;
    }

    printf("Device was lost for reason 0x%x with message %s\n", reason, msg);
    exit(1);
}

static void error_callback(WGPUErrorType type, const char* msg, void*) {
    printf("WGPU error for reason 0x%x with message %s\n", type, msg);
    exit(1);
}

static void queue_work_done_callback(WGPUQueueWorkDoneStatus status, void*, void*) {
    if (status == WGPUQueueWorkDoneStatus_Success) {
        return;
    }

    printf("Queue work submitted with status 0x%x\n", status);
    exit(1);
}

static result_t create_shader_module(const char* path, WGPUShaderModule* shader_module) {
    if (access(path, F_OK) != 0) {
        return result_file_access_failure;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return result_file_open_failure;
    }

    struct stat st;
    stat(path, &st);

    size_t num_bytes = (size_t)st.st_size;

    uint32_t* bytes = malloc(num_bytes);
    memset(bytes, 0, num_bytes);
    if (fread(bytes, num_bytes, 1, file) != 1) {
        return result_file_read_failure;
    }

    fclose(file);

    WGPUShaderModule module = wgpuDeviceCreateShaderModule(device, &(WGPUShaderModuleDescriptor) {
        .nextInChain = (const WGPUChainedStruct*) (&(WGPUShaderModuleSPIRVDescriptor) {
            .chain = {
                .sType = WGPUSType_ShaderModuleSPIRVDescriptor
            },
            .codeSize = (uint32_t) num_bytes / sizeof(uint32_t),
            .code = bytes
        })
    });

    if (module == NULL) {
        return result_shader_module_create_failure;
    }

    *shader_module = module;

    return result_success;
}

static WGPURequiredLimits get_required_limits(const WGPUSupportedLimits* limits, uint32_t surface_width, uint32_t surface_height) {
    return (WGPURequiredLimits) {
        .limits = {
            .maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED,
            .maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED,
            .maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED,
            .maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED,
            .maxBindGroups = WGPU_LIMIT_U32_UNDEFINED,
            .maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED,
            .maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED,
            .maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED,
            .maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED,
            .maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
            .maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
            .maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
            .maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
            .maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
            .maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED,
            .maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED,
            .minUniformBufferOffsetAlignment = limits->limits.minUniformBufferOffsetAlignment,
            .minStorageBufferOffsetAlignment = limits->limits.minStorageBufferOffsetAlignment,
            .maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED,
            .maxBufferSize = WGPU_LIMIT_U64_UNDEFINED,
            .maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED,
            .maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED,
            .maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED,
            .maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED,
            .maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED,
            .maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED,
            .maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED,
            .maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED,
            .maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED,
            .maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED,
            .maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED,
            .maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED,

            .maxVertexAttributes = 2,
            .maxVertexBuffers = 1,
            .maxBufferSize = sizeof(vertices),
            .maxVertexBufferArrayStride = sizeof(vertex_t),
            .maxBindGroups = 1,
            .maxUniformBuffersPerShaderStage = 1,
            .maxUniformBufferBindingSize = 16 * 4,
            .maxDynamicUniformBuffersPerPipelineLayout = 1,
            .maxTextureDimension1D = surface_width,
            .maxTextureDimension2D = surface_height,
            .maxSamplersPerShaderStage = 1
        }
    };
}

static uint32_t ceil_to_next_multiple(uint32_t value, uint32_t step) {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
}

static result_t init_wgpu_core(void) {
    result_t result;

    if ((instance = wgpuCreateInstance(&(WGPUInstanceDescriptor) {
    })) == NULL) {
        return result_instance_create_failure;
    }

    if ((surface = glfwGetWGPUSurface(instance, window)) == NULL) {
        return result_surface_get_failure;
    }

    wgpuInstanceRequestAdapter(instance, &(WGPURequestAdapterOptions) {
        .compatibleSurface = surface
    }, request_adapter, NULL);
    if (adapter == NULL) {
        return result_adapter_request_failure;
    }

    WGPUAdapterInfo adapter_info = {};
    wgpuAdapterGetInfo(adapter, &adapter_info);
    if (adapter_info.device) {
        printf("Adapter name: %s\n", adapter_info.device);
    }

    WGPUSupportedLimits adapter_supported_limits = {};
    if (wgpuAdapterGetLimits(adapter, &adapter_supported_limits) != WGPUStatus_Success) {
        return result_adapter_limits_get_failure;
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    float x_scale;
    float y_scale;

    glfwGetMonitorContentScale(monitor, &x_scale, &y_scale);

    uint32_t surface_width = WINDOW_WIDTH * (uint32_t) x_scale;
    uint32_t surface_height = WINDOW_HEIGHT * (uint32_t) y_scale;

    WGPURequiredLimits required_limits = get_required_limits(&adapter_supported_limits, surface_width, surface_height);

    wgpuAdapterRequestDevice(adapter, &(WGPUDeviceDescriptor) {
        .requiredFeatureCount = 0,
        .requiredLimits = &required_limits,
        .defaultQueue = {},
        .deviceLostCallbackInfo = {
            .callback = device_lost_callback,
            .mode = WGPUCallbackMode_AllowSpontaneous
        },
        .uncapturedErrorCallbackInfo = {
            .callback = error_callback
        }
    }, request_device, NULL);
    if (device == NULL) {
        return result_device_request_failure;
    }

    if (wgpuDeviceGetLimits(device, &device_limits) != WGPUStatus_Success) {
        return result_device_limits_get_failure;
    }

    WGPUSurfaceCapabilities surface_capabilities;
    if (wgpuSurfaceGetCapabilities(surface, adapter, &surface_capabilities) != WGPUStatus_Success) {
        return result_surface_capabilities_get_failure;
    }

    WGPUTextureFormat surface_format = surface_capabilities.formats[0];

    wgpuSurfaceConfigure(surface, &(WGPUSurfaceConfiguration) {
        .width = surface_width,
        .height = surface_height,
        .format = surface_format,
        .viewFormatCount = 0,
        .viewFormats = NULL,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = device,
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = WGPUCompositeAlphaMode_Auto
    });

    wgpuAdapterRelease(adapter);

    if ((queue = wgpuDeviceGetQueue(device)) == NULL) {
        return result_queue_get_failure;
    }
    
    wgpuQueueOnSubmittedWorkDone2(queue, (WGPUQueueWorkDoneCallbackInfo2) {
        .callback = queue_work_done_callback,
        .mode = WGPUCallbackMode_AllowSpontaneous
    });

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

    if ((depth_texture = wgpuDeviceCreateTexture(device, &(WGPUTextureDescriptor) {
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_Depth24Plus,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .size = { surface_width, surface_height, 1 },
        .usage = WGPUTextureUsage_RenderAttachment,
        .viewFormatCount = 1,
        .viewFormats = (WGPUTextureFormat[1]) { WGPUTextureFormat_Depth24Plus }
    })) == NULL) {
        return result_texture_create_failure;
    }

    if ((depth_texture_view = wgpuTextureCreateView(depth_texture, &(WGPUTextureViewDescriptor) {
        .aspect = WGPUTextureAspect_DepthOnly,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = WGPUTextureFormat_Depth24Plus
    })) == NULL) {
        return result_texture_view_create_failure;
    }

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

static void term_wgpu_core(void) {
    wgpuDeviceTick(device);
    wgpuTextureViewRelease(texture_view);
    wgpuTextureRelease(texture);
    wgpuTextureViewRelease(depth_texture_view);
    wgpuSamplerRelease(sampler);
    wgpuTextureRelease(depth_texture);
    wgpuBindGroupRelease(bind_group);
    wgpuBufferRelease(vertex_buffer);
    wgpuBufferRelease(index_buffer);
    wgpuBufferRelease(uniform_buffer);
    wgpuRenderPipelineRelease(pipeline);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
}

static result_t init_glfw_core(void) {
    if (!glfwInit()) {
        return result_glfw_init_failure;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan", NULL, NULL);
    if (window == NULL) {
        return result_window_create_failure;
    }

    return result_success;
}

static void term_glfw_core(void) {
    glfwTerminate();
}

static result_t game_loop(void) {
    float time = 0.0f;

    uint32_t uniform_stride = ceil_to_next_multiple(sizeof(*uniforms), device_limits.limits.minUniformBufferOffsetAlignment);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        time += 0.01f;

        uniforms[0].offset = (vec2s) {{ 0.3f * cosf(time) - 0.5f, 0.3f * sinf(time) }};
        uniforms[1].offset = (vec2s) {{ 0.3f * cosf(time) + 0.5f, 0.3f * sinf(time) }};

        for (size_t i = 0; i < 2; i++) {
            wgpuQueueWriteBuffer(queue, uniform_buffer, uniform_stride * i, &uniforms[i], sizeof(uniforms[i]));
        }

        WGPUSurfaceTexture texture;
        wgpuSurfaceGetCurrentTexture(surface, &texture);

        if (texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            return result_surface_texture_get_failure;
        }

        WGPUTextureView surface_view = wgpuTextureCreateView(texture.texture, &(WGPUTextureViewDescriptor) {
            .format = wgpuTextureGetFormat(texture.texture),
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All
        });

        if (surface_view == NULL) {
            return result_surface_texture_view_create_failure;
        }

        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(device, &(WGPUCommandEncoderDescriptor) {
        });

        if (command_encoder == NULL) {
            return result_command_encoder_create_failure;
        }

        WGPURenderPassEncoder render_pass_encoder = wgpuCommandEncoderBeginRenderPass(command_encoder, &(WGPURenderPassDescriptor) {
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
                .view = surface_view,
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

        WGPUCommandBuffer command = wgpuCommandEncoderFinish(command_encoder, &(WGPUCommandBufferDescriptor) {
        });
        
        if (command == NULL) {
            return result_command_encoder_finish_failure;
        }

        wgpuCommandEncoderRelease(command_encoder);

        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);

        wgpuSurfacePresent(surface);

        wgpuTextureViewRelease(surface_view);
    }

    return result_success;
}

int main() {
    result_t result;
    if ((result = init_glfw_core()) != result_success) {
        print_result_error(result);
        return 1;
    }

    if ((result = init_wgpu_core()) != result_success) {
        print_result_error(result);
        return 1;
    }

    if ((result = game_loop()) != result_success) {
        print_result_error(result);
        return 1;
    }

    term_wgpu_core();
    term_glfw_core();

    return 0;
}