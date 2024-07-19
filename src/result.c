#include "result.h"
#include <stdio.h>
#include <string.h>

static const char* get_result_string(result_t result) {
    switch (result) {
        case result_failure: return "Generic failure";
        
        case result_instance_create_failure: return "Failed to create instance";

        case result_adapter_request_failure: return "Failed to request adapter";

        default: return NULL;
    }
}

void print_result_error(result_t result) {
    printf("%s\n", get_result_string(result));
}
