#include "result.h"
#include <stdio.h>
#include <string.h>

static const char* get_result_string(result_t result) {
    switch (result) {
        case result_failure: return "Generic failure";
        
        case result_window_create_failure: return "Failed to create window";
        case result_instance_create_failure: return "Failed to create instance";
        case result_render_pipeline_create_failure: return "Failed to create render pipeline";
        case result_command_encoder_create_failure: return "Failed to create command encoder";
        case result_render_pass_encoder_create_failure: return "Failed to render pass encoder";
        case result_surface_texture_view_create_failure: return "Failed to create surface texture view";
        case result_shader_module_create_failure: return "Failed to create shader module";
        case result_buffer_create_failure: return "Failed to create buffer";
        case result_bind_group_layout_create_failure: return "Failed to create bind group layout";
        case result_pipeline_layout_create_failure: return "Failed to create pipeline layout";
        case result_bind_group_create_failure: return "Failed to create bind group";

        case result_adapter_request_failure: return "Failed to request adapter";
        case result_device_request_failure: return "Failed to request device";

        case result_surface_get_failure: return "Failed to get surface";
        case result_queue_get_failure: return "Failed to get queue";
        case result_surface_texture_get_failure: return "Failed to get surface texture";
        case result_adapter_limits_get_failure: return "Failed to get adapter limits";

        case result_command_encoder_finish_failure: return "Failed to finish command encoder";

        case result_glfw_init_failure: return "Failed to initialize GLFW";

        case result_file_access_failure: return "Failed to access file";
        case result_file_open_failure: return "Failed to open file";
        case result_file_read_failure: return "Failed to read file";

        default: return NULL;
    }
}

void print_result_error(result_t result) {
    printf("%s\n", get_result_string(result));
}
