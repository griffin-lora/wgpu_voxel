#pragma once
#include "result.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

extern GLFWwindow* window;
extern VkDevice device;
extern VmaAllocator allocator;
extern VkQueue queue;
extern VkSurfaceFormatKHR surface_format;
extern VkFormat depth_image_format;

extern VkCommandPool transfer_command_pool;
extern VkCommandBuffer transfer_command_buffer;
extern VkFence transfer_fence;

extern VkSampleCountFlagBits render_multisample_flags;
extern VkRenderPass frame_render_pass;

result_t init_gfx(void);
result_t draw_gfx(void);
void term_gfx(void);