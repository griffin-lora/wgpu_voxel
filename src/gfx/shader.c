#include "shader.h"
#include "gfx/gfx.h"
#include "result.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stb/stb_image.h>
#include <string.h>

result_t create_shader_module(const char* path, WGPUShaderModule* shader_module) {
    if (access(path, F_OK) != 0) {
        return result_file_access_failure;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return result_file_open_failure;
    }

    struct stat st;
    stat(path, &st);

    size_t num_bytes = (size_t)st.st_size;

    uint32_t* bytes = malloc(num_bytes);
    memset(bytes, 0, num_bytes);
    if (fread(bytes, num_bytes, 1, file) != 1) {
        return result_file_read_failure;
    }

    fclose(file);

    WGPUShaderModule module = wgpuDeviceCreateShaderModule(device, &(WGPUShaderModuleDescriptor) {
        .nextInChain = (const WGPUChainedStruct*) (&(WGPUShaderModuleSPIRVDescriptor) {
            .chain = {
                .sType = WGPUSType_ShaderModuleSPIRVDescriptor
            },
            .codeSize = (uint32_t) num_bytes / sizeof(uint32_t),
            .code = bytes
        })
    });

    if (module == NULL) {
        return result_shader_module_create_failure;
    }

    *shader_module = module;

    return result_success;
}