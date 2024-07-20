#include "result.h"
#include <GLFW/glfw3native.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <webgpu/wgpu.h>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

static GLFWwindow* window;
static WGPUInstance instance;
static WGPUSurface surface;
static WGPUAdapter adapter;
static WGPUDevice device;
static WGPUQueue queue;

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

static result_t init_wgpu_core(void) {
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

    wgpuAdapterRelease(adapter);

    wgpuDeviceSetUncapturedErrorCallback(device, error_callback, NULL);

    if ((queue = wgpuDeviceGetQueue(device)) == NULL) {
        return result_queue_get_failure;
    }
    
    wgpuQueueOnSubmittedWorkDone(queue, queue_work_done_callback, NULL);

    {
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &(WGPUCommandEncoderDescriptor) {
            .nextInChain = NULL,
            .label = "Debug Command Encoder"
        });

        if (encoder == NULL) {
            return result_command_encoder_create_failure;
        }

        wgpuCommandEncoderInsertDebugMarker(encoder, "Do first thing");
        wgpuCommandEncoderInsertDebugMarker(encoder, "Do second thing");

        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &(WGPUCommandBufferDescriptor) {
            .nextInChain = NULL,
            .label = "Command buffer"
        });
        
        if (command == NULL) {
            return result_command_encoder_finish_failure;
        }

        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }

    return result_success;
}

static void term_wgpu_core(void) {
    wgpuDevicePoll(device, true, NULL);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
}

static result_t init_glfw_core(void) {
    if (!glfwInit()) {
        return result_glfw_init_failure;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(640, 480, "Vulkan", NULL, NULL);
    if (window == NULL) {
        return result_window_create_failure;
    }

    return result_success;
}

static void term_glfw_core(void) {
    glfwTerminate();
}

static void game_loop(void) {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
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
    
    game_loop();

    term_wgpu_core();
    term_glfw_core();

    return 0;
}