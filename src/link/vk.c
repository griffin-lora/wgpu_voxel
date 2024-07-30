#include "gfx/gfx.h"
#include "vk_init.h"
#include <vulkan/vulkan.h>

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

static void (*vkCmdDrawMeshTasksNV_func)(VkCommandBuffer, uint32_t, uint32_t);

void vk_init_proc() {
    vkCmdDrawMeshTasksNV_func = vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksNV");
}

void vkCmdDrawMeshTasksNV(VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask) {
    vkCmdDrawMeshTasksNV_func(commandBuffer, taskCount, firstTask);
}