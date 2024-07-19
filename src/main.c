#include "result.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <wgpu.h>

static WGPUInstance instance;
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
    }
}

static result_t init(void) {
    if ((instance = wgpuCreateInstance(&(WGPUInstanceDescriptor) {
        .nextInChain = NULL
    })) == NULL) {
        return result_instance_create_failure;
    }

    wgpuInstanceRequestAdapter(instance, &(WGPURequestAdapterOptions) {
        .nextInChain = NULL
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

    queue = wgpuDeviceGetQueue(device);
    if (queue == NULL) {
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
    
    wgpuDevicePoll(device, true, NULL);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);

    return result_success;
}

int main() {
    result_t result;
    if ((result = init()) != result_success) {
        print_result_error(result);
        return 1;
    }

    return 0;
}