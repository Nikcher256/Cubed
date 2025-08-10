#pragma once

#include "Vulkan.h"
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

		void RenderCube(const glm::vec3& position, const glm::vec3& rotation, int textureIndex);
		void RenderUI();
		void OnSwapchainRecreated();

	private:
		VkShaderModule loadShader(const std::filesystem::path& path);
		void InitPipeline();
		void CreateRenderPass();
		void CreateDepthResources();
		void CreateFramebuffers();
		void InitBuffers();
		void CreateOrResizeBuffer(Buffer& buffer, uint64_t newSize);

		void DestroyPipeline();
		void DestroyFramebuffers();
		void DestroyDepthResources();
		void DestroyRenderPass();

		
	private:
		//buffers, pipelines
		VkPipeline m_GraphicsPipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;

		VkDescriptorSetLayout m_TexturesDescriptorSetLayout = nullptr;
		VkDescriptorSetLayout m_CameraDescriptorSetLayout = nullptr;
		VkDescriptorSet m_TexturesDescriptorSet = nullptr;
		VkDescriptorSet m_CameraDescriptorSet = nullptr;

		Buffer m_VertexBuffer, m_IndexBuffer, m_CameraUBO;

		struct PushConstants
		{
			glm::mat4 Transform;
			int TextureIndex = 0;
			int _pad[3];
		} m_PushConstants;

		struct CameraUBO {
			glm::mat4 ViewProjection;
		} m_CameraData;


		struct DepthResource {
			VkImage image = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkImageView view = VK_NULL_HANDLE;
		};

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		VkFormat     m_DepthFormat = VK_FORMAT_UNDEFINED;
		std::vector<DepthResource> m_Depth;
		std::vector<VkFramebuffer> m_Framebuffers;

	};
}