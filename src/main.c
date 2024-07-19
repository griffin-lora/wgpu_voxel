#include "result.h"
#include <stdio.h>
#include <webgpu.h>

static WGPUInstance instance;
static WGPUAdapter adapter;

#define MAKE_REQUEST_CALLBACK(TYPE, VAR) \
static void request_##VAR(WGPURequest##TYPE##Status, WGPU##TYPE local_##VAR, char const*, void*) { \
    VAR = local_##VAR; \
}

MAKE_REQUEST_CALLBACK(Adapter, adapter)

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
    
    wgpuInstanceRelease(instance);

    return result_success;
}

int main() {
    printf("Hello world!\n");

    result_t result;
    if ((result = init()) != result_success) {
        print_result_error(result);
        return 1;
    }

    return 0;
}