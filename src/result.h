#pragma once

typedef enum {
    result_success = 0,
    result_failure = 1,

    result_window_create_failure,
    result_instance_create_failure,
    result_command_encoder_create_failure,
    result_render_pass_encoder_create_failure,
    result_surface_texture_view_create_failure,
    
    result_adapter_request_failure,
    result_device_request_failure,

    result_surface_get_failure,
    result_queue_get_failure,
    result_surface_texture_get_failure,

    result_command_encoder_finish_failure,

    result_glfw_init_failure
} result_t;

void print_result_error(result_t result);
