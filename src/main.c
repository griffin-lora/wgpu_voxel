#include "camera.h"
#include "gfx/gfx.h"
#include "result.h"
#include <GLFW/glfw3.h>

int main() {
    result_t result;
    if ((result = init_gfx()) != result_success) {
        print_result_error(result);
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        camera_update();

        if ((result = draw_gfx()) != result_success) {
            print_result_error(result);
            return 1;
        }
    }

    term_gfx();

    return 0;
}