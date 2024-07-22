#include "gfx/gfx.h"
#include "result.h"


int main() {
    result_t result;
    if ((result = init_gfx()) != result_success) {
        print_result_error(result);
        return 1;
    }

    while (!should_window_close()) {
        if ((result = draw_gfx()) != result_success) {
            print_result_error(result);
            return 1;
        }
    }

    term_gfx();

    return 0;
}