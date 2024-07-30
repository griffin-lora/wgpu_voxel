#include "gfx/gfx.h"
#include "vk_init.h"
#include <vulkan/vulkan.h>

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

static PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT_Func;

void vk_init_proc() {
    vkCmdDrawMeshTasksEXT_Func = vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
}

void vkCmdDrawMeshTasksEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ) {
    vkCmdDrawMeshTasksEXT_Func(commandBuffer, groupCountX, groupCountY, groupCountZ);
}