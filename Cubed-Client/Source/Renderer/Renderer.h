#pragma once

#include "Vulkan.h"
#include "Texture.h"
#include <filesystem>
#include <glm/glm.hpp>

namespace Cubed {

	struct Buffer
	{
		VkBuffer Handle = nullptr;
		VkDeviceMemory Memory = nullptr;
		VkDeviceSize Size = 0;
		VkBufferUsageFlagBits Usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
	};

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 UV;
	};

	struct Camera
	{
		glm::vec3 Position{ 0, 0, 8 };
		glm::vec3 Rotation{ 0, 0, 0 };
	};

	class Renderer
	{
	public:
		void Init();
		void Shutdown();

		void BeginScene(const Camera& camera);
		void EndScene();

		void RenderCube(const glm::vec3& position, const glm::vec3& rotation);
		void RenderUI();
	public:
		static uint32_t GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
	private:
		VkShaderModule loadShader(const std::filesystem::path& path);
		void InitPipeline();
		void InitBuffers();
		void CreateOrResizeBuffer(Buffer& buffer, uint64_t newSize);
	private:
		//buffers, pipelines
		VkPipeline m_GraphicsPipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;

		VkDescriptorSetLayout m_DescriptorSetLayout = nullptr;
		VkDescriptorSet m_DescriptorSet = nullptr;

		Buffer m_VertexBuffer, m_IndexBuffer;

		struct PushConstants
		{
			glm::mat4 ViewProjection;
			glm::mat4 Transform;
		} m_PushConstants;

		std::shared_ptr<Texture> m_Texture;
	};
}