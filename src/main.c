#include "result.h"
#include <GLFW/glfw3native.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

static GLFWwindow* window;
static WGPUInstance instance;
static WGPUSurface surface;
static WGPUAdapter adapter;
static WGPUDevice device;
static WGPUQueue queue;
static WGPURenderPipeline pipeline;

#define MAKE_REQUEST_CALLBACK(TYPE, VAR) \
static void request_##VAR(WGPURequest##TYPE##Status, WGPU##TYPE local_##VAR, char const*, void*) { \
    VAR = local_##VAR; \
}

MAKE_REQUEST_CALLBACK(Adapter, adapter)
MAKE_REQUEST_CALLBACK(Device, device)

static void device_lost_callback(WGPUDeviceLostReason reason, const char* msg, void*) {
    printf("Device was lost for reason 0x%x with message %s\n", reason, msg);
    exit(1);
}

static void error_callback(WGPUErrorType type, const char* msg, void*) {
    printf("WGPU error for reason 0x%x with message %s\n", type, msg);
    exit(1);
}

static void queue_work_done_callback(WGPUQueueWorkDoneStatus status, void*) {
    if (status != WGPUQueueWorkDoneStatus_Success) {
        printf("Queue work submitted with status 0x%x\n", status);
        exit(1);
    }
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
        }),
        .hintCount = 0,
        .hints = NULL
    });

    if (module == NULL) {
        return result_shader_module_create_failure;
    }

    *shader_module = module;

    return result_success;
}

static result_t init_wgpu_core(void) {
    result_t result;

    if ((instance = wgpuCreateInstance(&(WGPUInstanceDescriptor) {
        .nextInChain = NULL
    })) == NULL) {
        return result_instance_create_failure;
    }

    if ((surface = glfwGetWGPUSurface(instance, window)) == NULL) {
        return result_surface_get_failure;
    }

    wgpuInstanceRequestAdapter(instance, &(WGPURequestAdapterOptions) {
        .nextInChain = NULL,
        .compatibleSurface = surface
    }, request_adapter, NULL);
    if (adapter == NULL) {
        return result_adapter_request_failure;
    }

    WGPUAdapterProperties adapter_properties = {
        .nextInChain = NULL
    };
    wgpuAdapterGetProperties(adapter, &adapter_properties);
    if (adapter_properties.name) {
        printf("Adapter name: %s\n", adapter_properties.name);
    }

    wgpuAdapterRequestDevice(adapter, &(WGPUDeviceDescriptor) {
        .nextInChain = NULL,
        .label = "Main Device",
        .requiredFeatureCount = 0,
        .requiredLimits = NULL,
        .defaultQueue = {
            .nextInChain = NULL,
            .label = "Default Queue"
        },
        .deviceLostCallback = device_lost_callback
    }, request_device, NULL);
    if (device == NULL) {
        return result_device_request_failure;
    }

    WGPUTextureFormat surface_format = wgpuSurfaceGetPreferredFormat(surface, adapter);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    float x_scale;
    float y_scale;

    glfwGetMonitorContentScale(monitor, &x_scale, &y_scale);

    wgpuSurfaceConfigure(surface, &(WGPUSurfaceConfiguration) {
        .nextInChain = NULL,
        .width = WINDOW_WIDTH * (uint32_t) x_scale,
        .height = WINDOW_HEIGHT * (uint32_t) y_scale,
        .format = surface_format,
        .viewFormatCount = 0,
        .viewFormats = NULL,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = device,
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = WGPUCompositeAlphaMode_Auto
    });

    wgpuAdapterRelease(adapter);

    wgpuDeviceSetUncapturedErrorCallback(device, error_callback, NULL);

    if ((queue = wgpuDeviceGetQueue(device)) == NULL) {
        return result_queue_get_failure;
    }
    
    wgpuQueueOnSubmittedWorkDone(queue, queue_work_done_callback, NULL);

    WGPUShaderModule vertex_shader_module;
    WGPUShaderModule fragment_shader_module;
    if ((result = create_shader_module("shader/vertex.spv", &vertex_shader_module)) != result_success) {
        return result;
    }
    if ((result = create_shader_module("shader/fragment.spv", &fragment_shader_module)) != result_success) {
        return result;
    }

    if ((pipeline = wgpuDeviceCreateRenderPipeline(device, &(WGPURenderPipelineDescriptor) {
        .nextInChain = NULL,
        .vertex = {
            .bufferCount = 0,
            .buffers = NULL,
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
        .depthStencil = NULL,
        .layout = NULL
    })) == NULL) {
        return result_render_pipeline_create_failure;
    }

    // wgpuShaderModuleRelease(vertex_shader_module);
    // wgpuShaderModuleRelease(fragment_shader_module);

    return result_success;
}

static void term_wgpu_core(void) {
    wgpuDevicePoll(device, true, NULL);
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
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        WGPUSurfaceTexture texture;
        wgpuSurfaceGetCurrentTexture(surface, &texture);

        if (texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            return result_surface_texture_get_failure;
        }

        WGPUTextureView surface_view = wgpuTextureCreateView(texture.texture, &(WGPUTextureViewDescriptor) {
            .nextInChain = NULL,
            .label = "Surface Texture View",
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
            .nextInChain = NULL,
            .label = "Render Command Encoder"
        });

        if (command_encoder == NULL) {
            return result_command_encoder_create_failure;
        }

        WGPURenderPassEncoder render_pass_encoder = wgpuCommandEncoderBeginRenderPass(command_encoder, &(WGPURenderPassDescriptor) {
            .nextInChain = NULL,
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
                .view = surface_view,
                .resolveTarget = NULL,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue = (WGPUColor) { 0.9, 0.1, 0.2, 1.0 }
            },
            .depthStencilAttachment = NULL,
            .timestampWrites = NULL // TODO: Implement later
        });

        if (render_pass_encoder == NULL) {
            return result_render_pass_encoder_create_failure;
        }

        wgpuRenderPassEncoderSetPipeline(render_pass_encoder, pipeline);
        wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);

        wgpuRenderPassEncoderEnd(render_pass_encoder);
        wgpuRenderPassEncoderRelease(render_pass_encoder);

        WGPUCommandBuffer command = wgpuCommandEncoderFinish(command_encoder, &(WGPUCommandBufferDescriptor) {
            .nextInChain = NULL,
            .label = "Render Command buffer"
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