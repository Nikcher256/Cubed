#include "Texture.h"
#include "Renderer.h"
#include "Walnut/Application.h"

namespace Cubed {

	Texture::Texture(uint32_t width, uint32_t height, Walnut::Buffer data) : m_Width(width), m_Height(height)
	{
		Init(data);
	}

	Texture::~Texture()
	{
		VkDevice device = Walnut::Application::GetDevice();

		vkDestroySampler(device, m_Sampler, nullptr);
		vkDestroyImageView(device, m_ImageView, nullptr);
		vkDestroyImage(device, m_Image, nullptr);
		vkFreeMemory(device, m_Memory, nullptr);
	}

	void Texture::Init(Walnut::Buffer data)
	{
		VkDevice device = GetVulkanInfo()->Device;

		size_t size = m_Width * m_Height * 4;
		if (size != data.Size)
		{
			__debugbreak();
			return;
		}

		// Create the Image:
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = VK_FORMAT_R8G8B8A8_UNORM;
			info.extent.width = m_Width;
			info.extent.height = m_Height;
			info.extent.depth = 1;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VK_CHECK(vkCreateImage(device, &info, nullptr, &m_Image));

			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(device, m_Image, &req);
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = Renderer::GetVulkanMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
			VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &m_Memory));
			VK_CHECK(vkBindImageMemory(device, m_Image, m_Memory, 0));
		}

		// Create the Image View:
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = m_Image;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = VK_FORMAT_R8G8B8A8_UNORM;
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.layerCount = 1;
			VK_CHECK(vkCreateImageView(device, &info, nullptr, &m_ImageView));
		}

		VkBuffer stagingBuffer = nullptr;
		VkDeviceMemory stagingBufferMemory = nullptr;

		// Create the Upload Buffer:
		{
			VkBufferCreateInfo buffer_info = {};
			buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_info.size = size;
			buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &stagingBuffer));
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(device, stagingBuffer, &req);
			// bd->BufferMemoryAlignment = (bd->BufferMemoryAlignment > req.alignment) ? bd->BufferMemoryAlignment : req.alignment;
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = Renderer::GetVulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
			VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &stagingBufferMemory));
			VK_CHECK(vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0));
		}

		// Upload to Buffer:
		{
			uint8_t* map = NULL;
			VK_CHECK(vkMapMemory(device, stagingBufferMemory, 0, size, 0, (void**)(&map)));
			memcpy(map, data.Data, size);
			VkMappedMemoryRange range[1] = {};
			range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range[0].memory = stagingBufferMemory;
			range[0].size = size;
			VK_CHECK(vkFlushMappedMemoryRanges(device, 1, range));
			vkUnmapMemory(device, stagingBufferMemory);
		}

		VkCommandBuffer commandBuffer = Walnut::Application::GetCommandBuffer(true);

		// Copy to Image:
		{
			VkImageMemoryBarrier copy_barrier[1] = {};
			copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copy_barrier[0].image = m_Image;
			copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy_barrier[0].subresourceRange.levelCount = 1;
			copy_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

			VkBufferImageCopy region = {};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = m_Width;
			region.imageExtent.height = m_Height;
			region.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			VkImageMemoryBarrier use_barrier[1] = {};
			use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].image = m_Image;
			use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			use_barrier[0].subresourceRange.levelCount = 1;
			use_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
		}

		Walnut::Application::FlushCommandBuffer(commandBuffer);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

		//sampler
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.minLod = -1000;
		info.maxLod = 1000;
		info.maxAnisotropy = 1.0f;
		VK_CHECK(vkCreateSampler(device, &info, nullptr, &m_Sampler));

		m_ImageInfo.imageView = m_ImageView;
		m_ImageInfo.sampler = m_Sampler;
		m_ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

}