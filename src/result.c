#include "result.h"
#include <stdio.h>
#include <string.h>

static const char* get_result_string(result_t result) {
    switch (result) {
        case result_failure: return "Generic failure";
        
        case result_window_create_failure: return "Failed to create window";
        case result_instance_create_failure: return "Failed to create instance";
        case result_command_encoder_create_failure: return "Failed to create command encoder";

        case result_adapter_request_failure: return "Failed to request adapter";
        case result_device_request_failure: return "Failed to request device";

        case result_surface_get_failure: return "Failed to get surface";
        case result_queue_get_failure: return "Failed to get queue";

        case result_command_encoder_finish_failure: return "Failed to finish command encoder";

        case result_glfw_init_failure: return "Failed to initialize GLFW";

        default: return NULL;
    }
}

void print_result_error(result_t result) {
    printf("%s\n", get_result_string(result));
}
