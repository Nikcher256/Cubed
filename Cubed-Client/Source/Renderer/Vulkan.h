#pragma once

#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <string>

namespace vkb {

	const std::string to_string(VkResult result);

}

namespace Cubed {
	ImGui_ImplVulkan_InitInfo* GetVulkanInfo();
	uint32_t GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
}

#define VK_CHECK(x)                                                                    \
	do                                                                                 \
	{                                                                                  \
		VkResult err = x;                                                              \
		if (err)                                                                       \
		{                                                                              \
			std::cout << "Detected Vulkan error: " << vkb::to_string(err) << std::endl; \
		}                                                                              \
	} while (0)
