#pragma once

typedef enum {
    result_success = 0,
    result_failure = 1,

    result_instance_create_failure,
    
    result_adapter_request_failure
} result_t;

void print_result_error(result_t result);
