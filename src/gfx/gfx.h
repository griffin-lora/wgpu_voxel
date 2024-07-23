#pragma once
#include "result.h"
#include <GLFW/glfw3.h>
#include <dawn/webgpu.h>

extern GLFWwindow* window;
extern WGPUDevice device;
extern WGPUQueue queue;
extern WGPUSupportedLimits device_limits;
extern WGPUTextureFormat surface_format;

result_t init_gfx(void);
result_t draw_gfx(void);
void term_gfx(void);