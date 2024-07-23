#pragma once
#include "result.h"
#include <dawn/webgpu.h>

result_t create_shader_module(const char* path, WGPUShaderModule* shader_module);