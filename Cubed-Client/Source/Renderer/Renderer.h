#pragma once

#include "../Assets/Model.h"
#include "../Assets/ModelManager.h"

#include "Vulkan.h"
#include <filesystem>
#include <glm/glm.hpp>


namespace Cubed {

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

		void AddModel(std::shared_ptr<Cubed::Model> m) { m_Models.push_back(std::move(m)); }
		void RenderModels();
	private:
		VkShaderModule loadShader(const std::filesystem::path& path);
		void InitPipeline();
		void CreateRenderPass();
		void CreateDepthResources();
		void CreateFramebuffers();
		void InitBuffers();
		void CreateOrResizeBuffer(Buffer& buffer, uint64_t newSize);
		void CreateTextureDescriptorSet(uint32_t maxTexId);
		void CreateCameraDescriptorSet();
		void LogModelInfo(const std::shared_ptr<Cubed::Model>& model);

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

		struct PushConstants {
			glm::mat4 Transform;   // 64
			int TextureIndex;          // 4
			int IsOutline;         // 4  (0/1)
			float OutlineThickness;// 4  (in world units)
			int _pad;              // 4  (keep 16B alignment)
		}m_PushConstants;

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

		std::vector<std::shared_ptr<Cubed::Model>> m_Models;

	};
}