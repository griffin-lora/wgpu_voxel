#include "gfx.h"
#include "gfx/compute_pipeline.h"
#include "gfx/render_pipeline.h"
#include "glfw3webgpu.h"
#include "result.h"
#include <cglm/types-struct.h>
#include <stdio.h>
#include <stdlib.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

GLFWwindow* window;
static WGPUInstance instance;
static WGPUSurface surface;
static WGPUAdapter adapter;
WGPUDevice device;
WGPUQueue queue;
WGPUSupportedLimits device_limits;
WGPUTextureFormat surface_format;
static WGPUTexture depth_texture;
static WGPUTextureView depth_texture_view;

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

            // TODO: Add a way to get limits from other files
            .maxVertexAttributes = 2,
            .maxVertexBuffers = 1,
            .maxBufferSize = 128,
            .maxVertexBufferArrayStride = 64,
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

    surface_format = surface_capabilities.formats[0];

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

    if ((result = init_render_pipeline()) != result_success) {
        return result;
    }

    if ((result = init_compute_pipeline()) != result_success) {
        return result;
    }
    
    return result_success;
}

static void term_wgpu_core(void) {
    wgpuDeviceTick(device);
    term_render_pipeline();
    term_compute_pipeline();

    wgpuTextureViewRelease(depth_texture_view);
    wgpuTextureRelease(depth_texture);
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

result_t draw_gfx(void) {
    result_t result;

    glfwPollEvents();

    WGPUSurfaceTexture texture;
    wgpuSurfaceGetCurrentTexture(surface, &texture);

    if (texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return result_surface_texture_get_failure;
    }

    WGPUTextureView surface_texture_view = wgpuTextureCreateView(texture.texture, &(WGPUTextureViewDescriptor) {
        .format = wgpuTextureGetFormat(texture.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All
    });

    if (surface_texture_view == NULL) {
        return result_surface_texture_view_create_failure;
    }

    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(device, &(WGPUCommandEncoderDescriptor) {
    });

    if (command_encoder == NULL) {
        return result_command_encoder_create_failure;
    }

    if ((result = draw_render_pipeline(command_encoder, surface_texture_view, depth_texture_view)) != result_success) {
        return result;
    }

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(command_encoder, &(WGPUCommandBufferDescriptor) {
    });
    
    if (command == NULL) {
        return result_command_encoder_finish_failure;
    }

    wgpuCommandEncoderRelease(command_encoder);

    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuSurfacePresent(surface);

    wgpuTextureViewRelease(surface_texture_view);

    return result_success;
}

result_t init_gfx() {
    result_t result;

    if ((result = init_glfw_core()) != result_success) {
        return result;
    }

    if ((result = init_wgpu_core()) != result_success) {
        return result;
    }

    return result_success;
}

bool should_window_close() {
    return glfwWindowShouldClose(window);
}

void term_gfx() {
    term_wgpu_core();
    term_glfw_core();
}