#include "Renderer.h"

#include <array>	
#include <fstream>
#include <vector>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Walnut/Application.h"
#include "Walnut/Core/Log.h"
#include "../../../Walnut/vendor/stb_image/stb_image.h"
#include "../Assets/TextureManager.h"



namespace Cubed {


	void Renderer::Init()
	{
		//// Base textures
		const std::filesystem::path TEXTURE_BASE_PATH = "C:/Users/Asus/Documents/Projects/Cubed/Cubed-Client/Assets/Textures";
		TextureManager::LoadTexture(TEXTURE_BASE_PATH / "simple.png"); // id 0
		//TextureManager::LoadTexture(TEXTURE_BASE_PATH / "man.png");    // id 1

		//// Load model first (so we know max texture index)
		//auto model = Cubed::ModelManager::Load(
		//	"C:/Users/Asus/Documents/Projects/Cubed/Cubed-Client/Assets/Models/tank.glb"
		//);
		//model->SetSizeMeters(4.0f); // Set size to 1 meter
		//model->SetRotation(180.0f, glm::vec3(0, 1, 0)); // Rotate 90 degrees around Y-axis
		//model->SetPosition(glm::vec3(0, -2, 0)); // Center position

		//AddModel(model);

		//auto model1 = Cubed::ModelManager::Load(
		//	"C:/Users/Asus/Documents/Projects/Cubed/Cubed-Client/Assets/Models/miyako.glb"
		//);
		//model1->SetSizeMeters(4.0f); // Set size to 1 meter
		////model1->SetRotation(-90.0f, glm::vec3(0, 1, 0)); // Rotate 90 degrees around Y-axis
		//model1->SetPosition(glm::vec3(4, -2, 0)); // Center position

		//AddModel(model1);

		//// Log model info
		//LogModelInfo(model);

		// Find max texture ID across all models
		uint32_t maxTexId = 0;
		for (auto& m : m_Models)
			for (auto& mesh : m->GetMeshes())
				if (mesh.TextureIndex > maxTexId) maxTexId = mesh.TextureIndex;

		// Create descriptor sets
		CreateTextureDescriptorSet(maxTexId);
		CreateCameraDescriptorSet();

		// Render infra
		CreateRenderPass();
		CreateDepthResources();
		CreateFramebuffers();
		InitBuffers();
		InitPipeline();
	}

	void Renderer::CreateTextureDescriptorSet(uint32_t maxTexId)
	{
		VkDevice device = GetVulkanInfo()->Device;
		static constexpr uint32_t MAX_TEXTURES = 100;

		// Layout for texture array
		VkDescriptorSetLayoutBinding texBinding{};
		texBinding.binding = 0;
		texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texBinding.descriptorCount = MAX_TEXTURES;
		texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo texLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		texLayoutInfo.bindingCount = 1;
		texLayoutInfo.pBindings = &texBinding;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &texLayoutInfo, nullptr, &m_TexturesDescriptorSetLayout));

		m_TexturesDescriptorSet = Walnut::Application::AllocateDescriptorSet(m_TexturesDescriptorSetLayout);

		// Fill descriptors
		std::vector<VkDescriptorImageInfo> imageInfos(MAX_TEXTURES);
		for (uint32_t i = 0; i < MAX_TEXTURES; ++i) {
			if (i < TextureManager::Count()) {
				imageInfos[i] = TextureManager::GetTexture(i)->GetImageInfo();
				WL_INFO_TAG("CLIENT", "USING normal texture");
			}
			else
			{ 
				WL_INFO_TAG("CLIENT", "USING 0 texture");
				imageInfos[i] = TextureManager::GetTexture(0)->GetImageInfo();
			}
		}

		VkWriteDescriptorSet texWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		texWrite.dstSet = m_TexturesDescriptorSet;
		texWrite.dstBinding = 0;
		texWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texWrite.descriptorCount = MAX_TEXTURES;
		texWrite.pImageInfo = imageInfos.data();
		vkUpdateDescriptorSets(device, 1, &texWrite, 0, nullptr);
	}

	void Renderer::CreateCameraDescriptorSet()
	{
		VkDevice device = GetVulkanInfo()->Device;

		// Layout
		VkDescriptorSetLayoutBinding camBinding{};
		camBinding.binding = 0;
		camBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		camBinding.descriptorCount = 1;
		camBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo camLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		camLayoutInfo.bindingCount = 1;
		camLayoutInfo.pBindings = &camBinding;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &camLayoutInfo, nullptr, &m_CameraDescriptorSetLayout));

		// Buffer
		m_CameraUBO.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		CreateOrResizeBuffer(m_CameraUBO, sizeof(m_CameraData));

		// Descriptor set
		m_CameraDescriptorSet = Walnut::Application::AllocateDescriptorSet(m_CameraDescriptorSetLayout);

		// Write descriptor
		VkDescriptorBufferInfo camBufInfo{};
		camBufInfo.buffer = m_CameraUBO.Handle;
		camBufInfo.offset = 0;
		camBufInfo.range = sizeof(m_CameraData);

		VkWriteDescriptorSet camWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		camWrite.dstSet = m_CameraDescriptorSet;
		camWrite.dstBinding = 0;
		camWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		camWrite.descriptorCount = 1;
		camWrite.pBufferInfo = &camBufInfo;
		vkUpdateDescriptorSets(device, 1, &camWrite, 0, nullptr);
	}

	void Renderer::LogModelInfo(const std::shared_ptr<Cubed::Model>& model)
	{
		const auto& meshes = model->GetMeshes();
		std::cout << "[Model Info] Mesh count: " << meshes.size() << "\n";
		for (size_t i = 0; i < meshes.size(); ++i) {
			std::cout << "  Mesh(" << meshes[i].Name << ")" << i
				<< " - TextureIndex: " << meshes[i].TextureIndex;
			if (meshes[i].TextureIndex < TextureManager::Count()) {
				auto tex = TextureManager::GetTexture(meshes[i].TextureIndex);
				std::cout << " (texture loaded)\n";
			}
			else {
				std::cout << " (no texture)\n";
			}
		}
	}



	void Renderer::Shutdown() {
		VkDevice device = GetVulkanInfo()->Device;
		vkDeviceWaitIdle(device);

		DestroyPipeline();
		DestroyFramebuffers();
		DestroyDepthResources();
		DestroyRenderPass();
		TextureManager::ClearCache();
		for (auto& model : m_Models) {
			model->DestroyGPU();
		}
		ModelManager::Clear();

		// Buffers
		if (m_VertexBuffer.Handle) { vkDestroyBuffer(device, m_VertexBuffer.Handle, nullptr); m_VertexBuffer.Handle = VK_NULL_HANDLE; }
		if (m_VertexBuffer.Memory) { vkFreeMemory(device, m_VertexBuffer.Memory, nullptr);   m_VertexBuffer.Memory = VK_NULL_HANDLE; }
		if (m_IndexBuffer.Handle) { vkDestroyBuffer(device, m_IndexBuffer.Handle, nullptr);  m_IndexBuffer.Handle = VK_NULL_HANDLE; }
		if (m_IndexBuffer.Memory) { vkFreeMemory(device, m_IndexBuffer.Memory, nullptr);     m_IndexBuffer.Memory = VK_NULL_HANDLE; }
		if (m_CameraUBO.Handle) { vkDestroyBuffer(device, m_CameraUBO.Handle, nullptr);    m_CameraUBO.Handle = VK_NULL_HANDLE; }
		if (m_CameraUBO.Memory) { vkFreeMemory(device, m_CameraUBO.Memory, nullptr);       m_CameraUBO.Memory = VK_NULL_HANDLE; }

		// Descriptor sets/layouts (optional to free sets if pool is reset elsewhere)
		if (m_TexturesDescriptorSet) {
			vkFreeDescriptorSets(device, Walnut::Application::GetDescriptorPool(), 1, &m_TexturesDescriptorSet);
			m_TexturesDescriptorSet = VK_NULL_HANDLE;
		}
		if (m_CameraDescriptorSet) {
			vkFreeDescriptorSets(device, Walnut::Application::GetDescriptorPool(), 1, &m_CameraDescriptorSet);
			m_CameraDescriptorSet = VK_NULL_HANDLE;
		}
		if (m_TexturesDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, m_TexturesDescriptorSetLayout, nullptr); m_TexturesDescriptorSetLayout = VK_NULL_HANDLE; }
		if (m_CameraDescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, m_CameraDescriptorSetLayout, nullptr);   m_CameraDescriptorSetLayout = VK_NULL_HANDLE; }
	}


	// In Renderer.cpp
	void Renderer::OnSwapchainRecreated() {
		VkDevice device = GetVulkanInfo()->Device;
		vkDeviceWaitIdle(device);

		// Tear down swapchain-dependent stuff
		DestroyPipeline();
		DestroyFramebuffers();
		DestroyDepthResources();
		DestroyRenderPass();

		// Recreate with the new wd->Width/Height/SurfaceFormat
		CreateRenderPass();
		CreateDepthResources();
		CreateFramebuffers();
		InitPipeline(); // pipeline references m_RenderPass, so rebuild it
	}


	// In Renderer.cpp
	void Renderer::DestroyPipeline() {
		VkDevice device = GetVulkanInfo()->Device;
		if (m_GraphicsPipeline) { vkDestroyPipeline(device, m_GraphicsPipeline, nullptr); m_GraphicsPipeline = VK_NULL_HANDLE; }
		if (m_PipelineLayout) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
	}

	void Renderer::DestroyFramebuffers() {
		VkDevice device = GetVulkanInfo()->Device;
		for (auto fb : m_Framebuffers)
			if (fb) vkDestroyFramebuffer(device, fb, nullptr);
		m_Framebuffers.clear();
	}

	void Renderer::DestroyDepthResources() {
		VkDevice device = GetVulkanInfo()->Device;
		for (auto& d : m_Depth) {
			if (d.view) { vkDestroyImageView(device, d.view, nullptr);   d.view = VK_NULL_HANDLE; }
			if (d.image) { vkDestroyImage(device, d.image, nullptr);      d.image = VK_NULL_HANDLE; }
			if (d.memory) { vkFreeMemory(device, d.memory, nullptr);       d.memory = VK_NULL_HANDLE; }
		}
		m_Depth.clear();
	}

	void Renderer::DestroyRenderPass() {
		VkDevice device = GetVulkanInfo()->Device;
		if (m_RenderPass) { vkDestroyRenderPass(device, m_RenderPass, nullptr); m_RenderPass = VK_NULL_HANDLE; }
	}


	void Renderer::BeginScene(const Camera& camera) {
		auto* wd = Walnut::Application::GetMainWindowData();
		VkCommandBuffer cmd = Walnut::Application::GetActiveCommandBuffer();
		VkDevice device = GetVulkanInfo()->Device;

		// --- update camera UBO (your code) ---
		float w = (float)wd->Width, h = (float)wd->Height;
		glm::mat4 camXf = glm::translate(glm::mat4(1.0f), camera.Position) *
			glm::eulerAngleXYZ(glm::radians(camera.Rotation.x),
				glm::radians(camera.Rotation.y),
				glm::radians(camera.Rotation.z));
		m_CameraData.ViewProjection =
			glm::perspectiveRH_ZO(glm::radians(45.0f), w / h, 0.1f, 1000.0f) * glm::inverse(camXf);

		void* data;
		vkMapMemory(device, m_CameraUBO.Memory, 0, sizeof(m_CameraData), 0, &data);
		memcpy(data, &m_CameraData, sizeof(m_CameraData));
		vkUnmapMemory(device, m_CameraUBO.Memory);

		// --- begin your render pass ---
		uint32_t frameIndex = wd->FrameIndex;

		VkClearValue clears[2];
        clears[0].color = { {0.53f, 0.81f, 0.98f, 1.0f} };
		clears[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo rp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rp.renderPass = m_RenderPass;
		rp.framebuffer = m_Framebuffers[frameIndex];
		rp.renderArea.offset = { 0,0 };
		rp.renderArea.extent = { (uint32_t)wd->Width, (uint32_t)wd->Height };
		rp.clearValueCount = 2;
		rp.pClearValues = clears;

		vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport vp{ 0, (float)wd->Height, (float)wd->Width, -(float)wd->Height, 0.0f, 1.0f };
		vkCmdSetViewport(cmd, 0, 1, &vp);
		VkRect2D sc{ {0,0}, { (uint32_t)wd->Width, (uint32_t)wd->Height } };
		vkCmdSetScissor(cmd, 0, 1, &sc);
	}

	void Renderer::EndScene() {
		VkCommandBuffer cmd = Walnut::Application::GetActiveCommandBuffer();
		vkCmdEndRenderPass(cmd);
	}

	void Renderer::RenderCube(const glm::vec3& position, const glm::vec3& rotation, int textureIndex)
	{
		glm::vec3 translation = position;

		m_PushConstants.Transform = glm::translate(glm::mat4(1.0f), translation) *
			glm::eulerAngleXYZ(glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z));
		m_PushConstants.TextureIndex = textureIndex;

		VkCommandBuffer commandBuffer = Walnut::Application::GetActiveCommandBuffer();

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		VkDescriptorSet sets[] = { m_TexturesDescriptorSet, m_CameraDescriptorSet };
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
			0, 2, sets, 0, nullptr);

		vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &m_PushConstants);

		VkDeviceSize offset{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VertexBuffer.Handle, &offset);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.Handle, offset, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, (uint32_t)m_IndexBuffer.Size / 4, 1, 0, 0, 0);
	}

	void Renderer::RenderModels()
	{
		VkCommandBuffer cmd = Walnut::Application::GetActiveCommandBuffer();

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		VkDescriptorSet sets[] = { m_TexturesDescriptorSet, m_CameraDescriptorSet };

		for (auto& model : m_Models)
		{
			for (const auto& mesh : model->GetMeshes())
			{
				// Bind descriptor sets once per draw
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
					0, 2, sets, 0, nullptr);

				// Detect outline meshes by name
				bool isOutline = (mesh.Name.find("outline") != std::string::npos);

				// Fill push constants
				m_PushConstants.Transform = model->GetTransform();
				m_PushConstants.TextureIndex = static_cast<int>(mesh.TextureIndex);
				m_PushConstants.IsOutline = isOutline ? 1 : 0;
				m_PushConstants.OutlineThickness = isOutline ? 0.0f : 0.0f; // meters
				m_PushConstants._pad = 0;

				vkCmdPushConstants(cmd, m_PipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0, sizeof(PushConstants), &m_PushConstants);

				// Bind and draw
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.VertexBuffer.Handle, &offset);
				vkCmdBindIndexBuffer(cmd, mesh.IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, mesh.IndexCount, 1, 0, 0, 0);
			}
		}
	}


	void Renderer::RenderUI()
	{
		
	}

	void Renderer::CreateDepthResources() {
		auto* wd = Walnut::Application::GetMainWindowData();
		VkDevice device = GetVulkanInfo()->Device;

		m_Depth.resize(wd->ImageCount);

		for (uint32_t i = 0; i < wd->ImageCount; ++i) {
			VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			ici.imageType = VK_IMAGE_TYPE_2D;
			ici.format = m_DepthFormat;
			ici.extent = { (uint32_t)wd->Width, (uint32_t)wd->Height, 1 };
			ici.mipLevels = 1;
			ici.arrayLayers = 1;
			ici.samples = VK_SAMPLE_COUNT_1_BIT;
			ici.tiling = VK_IMAGE_TILING_OPTIMAL;
			ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			VK_CHECK(vkCreateImage(device, &ici, nullptr, &m_Depth[i].image));

			VkMemoryRequirements req{};
			vkGetImageMemoryRequirements(device, m_Depth[i].image, &req);

			VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			mai.allocationSize = req.size;
			mai.memoryTypeIndex = GetVulkanMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
			VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &m_Depth[i].memory));
			VK_CHECK(vkBindImageMemory(device, m_Depth[i].image, m_Depth[i].memory, 0));

			VkImageViewCreateInfo iv{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			iv.image = m_Depth[i].image;
			iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
			iv.format = m_DepthFormat;
			iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			iv.subresourceRange.levelCount = 1;
			iv.subresourceRange.layerCount = 1;
			VK_CHECK(vkCreateImageView(device, &iv, nullptr, &m_Depth[i].view));
		}
	}


	void Renderer::CreateFramebuffers() {
		auto* wd = Walnut::Application::GetMainWindowData();
		VkDevice device = GetVulkanInfo()->Device;

		m_Framebuffers.resize(wd->ImageCount);

		for (uint32_t i = 0; i < wd->ImageCount; ++i) {
			VkImageView attachments[2] = {
				wd->Frames[i].BackbufferView, // color
				m_Depth[i].view               // depth
			};

			VkFramebufferCreateInfo fbi{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			fbi.renderPass = m_RenderPass;
			fbi.attachmentCount = 2;
			fbi.pAttachments = attachments;
			fbi.width = (uint32_t)wd->Width;
			fbi.height = (uint32_t)wd->Height;
			fbi.layers = 1;

			VK_CHECK(vkCreateFramebuffer(device, &fbi, nullptr, &m_Framebuffers[i]));
		}
	}


	void Renderer::CreateRenderPass()
	{
		VkDevice device = GetVulkanInfo()->Device;

		VkSurfaceFormatKHR surfaceFormat = Walnut::Application::GetMainWindowData()->SurfaceFormat;

		// Color attachment (swapchain image format)
		VkAttachmentDescription colorAttachment{};
		// Renderer::CreateRenderPass()
		colorAttachment.format = surfaceFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;           // clear scene background
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;          // fresh image each frame
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkPhysicalDevice physicalDevice = GetVulkanInfo()->PhysicalDevice;

		VkFormat candidates[] = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT
		};

		m_DepthFormat = VK_FORMAT_UNDEFINED;

		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
			if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				m_DepthFormat = format;
				break;
			}
		}

		// Use depthFormat for depthAttachment.format


		// Depth attachment (typical depth format)
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = m_DepthFormat; // Make sure this format is supported on your GPU!
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear depth at start
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // No need to store depth
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Attachment references for the subpass
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0; // index in attachment descriptions array
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1; // second attachment
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Subpass description
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		// Subpass dependency to handle layout transitions and synchronization
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// Create render pass info
		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass));
	}


	void Renderer::InitPipeline()
	{

		VkDevice device = GetVulkanInfo()->Device;
		
		VkDescriptorSetLayout setLayouts[2] = { m_TexturesDescriptorSetLayout, m_CameraDescriptorSetLayout };

		std::array<VkPushConstantRange, 1> pushConstantRanges;
		pushConstantRanges[0].offset = 0;
		pushConstantRanges[0].size = sizeof(PushConstants);
		pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		layout_info.pPushConstantRanges = pushConstantRanges.data();
		layout_info.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
		layout_info.setLayoutCount = 2;
		layout_info.pSetLayouts = setLayouts;
		VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &m_PipelineLayout));

		std::array<VkVertexInputBindingDescription, 1> binding_desc;
		binding_desc[0].binding = 0;
		binding_desc[0].stride = sizeof(Vertex);
		binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::array<VkVertexInputAttributeDescription, 3> attribute_desc;
		// Position
		attribute_desc[0].location = 0;
		attribute_desc[0].binding = binding_desc[0].binding;
		attribute_desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_desc[0].offset = offsetof(Vertex, Position);

		// Normal
		attribute_desc[1].location = 1;
		attribute_desc[1].binding = binding_desc[0].binding;
		attribute_desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_desc[1].offset = offsetof(Vertex, Normal);

		// UV
		attribute_desc[2].location = 2;
		attribute_desc[2].binding = binding_desc[0].binding;
		attribute_desc[2].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_desc[2].offset = offsetof(Vertex, UV);

		VkPipelineVertexInputStateCreateInfo vertex_input = {};
		vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input.vertexBindingDescriptionCount = (uint32_t)binding_desc.size();
		vertex_input.pVertexBindingDescriptions = binding_desc.data();
		vertex_input.vertexAttributeDescriptionCount = (uint32_t)attribute_desc.size();
		vertex_input.pVertexAttributeDescriptions = attribute_desc.data();


		// Specify we will use triangle lists to draw geometry.
		VkPipelineInputAssemblyStateCreateInfo input_assembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

		// Specify rasterization state.
		VkPipelineRasterizationStateCreateInfo raster{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.lineWidth = 1.0f };

		// Our attachment will write to all color channels, but no blending is enabled.
		VkPipelineColorBlendAttachmentState blend_attachment{
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

		VkPipelineColorBlendStateCreateInfo blend{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &blend_attachment };

		// We will have one viewport and scissor box.
		VkPipelineViewportStateCreateInfo viewport{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1 };

		VkPipelineDepthStencilStateCreateInfo depth_stencil{};
		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil.depthTestEnable = VK_TRUE;
		depth_stencil.depthWriteEnable = VK_TRUE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS; // Standard depth test
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.stencilTestEnable = VK_FALSE;

		// No multisampling.
		VkPipelineMultisampleStateCreateInfo multisample{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };

		// Specify that these states will be dynamic, i.e. not part of pipeline state object.
		std::array<VkDynamicState, 2> dynamics{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamic{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
			.pDynamicStates = dynamics.data() };

		// Load our SPIR-V shaders.

		std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};


		const std::filesystem::path SHADER_BASE_PATH = "C:/Users/Asus/Documents/Projects/Cubed/Cubed-Client/Assets/Shaders/bin";


		shader_stages[0] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = loadShader((SHADER_BASE_PATH / "basic.vert.spirv").string()),
			.pName = "main"
		};

		shader_stages[1] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadShader((SHADER_BASE_PATH / "basic.frag.spirv").string()),
			.pName = "main"
		};


		VkGraphicsPipelineCreateInfo pipe{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = static_cast<uint32_t>(shader_stages.size()),
			.pStages = shader_stages.data(),
			.pVertexInputState = &vertex_input,
			.pInputAssemblyState = &input_assembly,
			.pViewportState = &viewport,
			.pRasterizationState = &raster,
			.pMultisampleState = &multisample,
			.pDepthStencilState = &depth_stencil,
			.pColorBlendState = &blend,
			.pDynamicState = &dynamic,
			.layout = m_PipelineLayout,        // We need to specify the pipeline layout up front
			.renderPass = m_RenderPass             // We need to specify the render pass up front
		};

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe, nullptr, &m_GraphicsPipeline));

		// Pipeline is baked, we can delete the shader modules now.
		vkDestroyShaderModule(device, shader_stages[0].module, nullptr);
		vkDestroyShaderModule(device, shader_stages[1].module, nullptr);
	}

	inline glm::vec2 UVFromBlock(int blockX, int blockY, float u, float v) {
		const float blockW = 1.0f / 3.0f;
		const float blockH = 1.0f / 4.0f;
		return glm::vec2(
			blockX * blockW + u * blockW,
			blockY * blockH + v * blockH
		);
	}

	void Renderer::InitBuffers()
	{
		VkDevice device = GetVulkanInfo()->Device;

		std::array<Vertex, 24> vertexData;
		// Front face (0,0,1)
		vertexData[0].Position = glm::vec3(-0.5f, -0.5f,  0.5f);
		vertexData[0].Normal  =  glm::vec3( 0.0f,  0.0f,  1.0f);
		vertexData[0].UV = UVFromBlock(1, 2, 0, 0);
		vertexData[1].Position = glm::vec3(-0.5f,  0.5f,  0.5f);
		vertexData[1].Normal  =  glm::vec3( 0.0f,  0.0f,  1.0f);
		vertexData[1].UV = UVFromBlock(1, 2, 0, 1);
		vertexData[2].Position = glm::vec3( 0.5f,  0.5f,  0.5f);
		vertexData[2].Normal  =  glm::vec3( 0.0f,  0.0f,  1.0f);
		vertexData[2].UV = UVFromBlock(1, 2, 1, 1);
		vertexData[3].Position = glm::vec3( 0.5f, -0.5f,  0.5f);
		vertexData[3].Normal  =  glm::vec3( 0.0f,  0.0f,  1.0f);
		vertexData[3].UV = UVFromBlock(1, 2, 1, 0);

		// Right face (1,0,0)
		vertexData[4].Position = glm::vec3(0.5f, -0.5f, 0.5f);
		vertexData[4].Normal = glm::vec3(1.0f, 0.0f, 0.0f);
		vertexData[4].UV = UVFromBlock(1, 0, 0, 1);
		vertexData[5].Position = glm::vec3(0.5f, 0.5f, 0.5f);
		vertexData[5].Normal = glm::vec3(1.0f, 0.0f, 0.0f);
		vertexData[5].UV = UVFromBlock(1, 0, 0, 0);
		vertexData[6].Position = glm::vec3(0.5f, 0.5f, -0.5f);
		vertexData[6].Normal = glm::vec3(1.0f, 0.0f, 0.0f);
		vertexData[6].UV = UVFromBlock(1, 0, 1, 0);
		vertexData[7].Position = glm::vec3(0.5f, -0.5f, -0.5f);
		vertexData[7].Normal = glm::vec3(1.0f, 0.0f, 0.0f);
		vertexData[7].UV = UVFromBlock(1, 0, 1, 1);

		// Back face (0,0,-1)
		vertexData[8].Position = glm::vec3(0.5f, -0.5f, -0.5f);
		vertexData[8].Normal = glm::vec3(0.0f, 0.0f, -1.0f);
		vertexData[8].UV = UVFromBlock(1, 0, 0, 1);
		vertexData[9].Position = glm::vec3(0.5f, 0.5f, -0.5f);
		vertexData[9].Normal = glm::vec3(0.0f, 0.0f, -1.0f);
		vertexData[9].UV = UVFromBlock(1, 0, 0, 0);
		vertexData[10].Position = glm::vec3(-0.5f, 0.5f, -0.5f);
		vertexData[10].Normal = glm::vec3(0.0f, 0.0f, -1.0f);
		vertexData[10].UV = UVFromBlock(1, 0, 1, 0);
		vertexData[11].Position = glm::vec3(-0.5f, -0.5f, -0.5f);
		vertexData[11].Normal = glm::vec3(0.0f, 0.0f, -1.0f);
		vertexData[11].UV = UVFromBlock(1, 0, 1, 1);

		// Left face (-1,0,0)
		vertexData[12].Position = glm::vec3(-0.5f, -0.5f, -0.5f);
		vertexData[12].Normal = glm::vec3(-1.0f, 0.0f, 0.0f);
		vertexData[12].UV = UVFromBlock(1, 2, 0, 0);
		vertexData[13].Position = glm::vec3(-0.5f, 0.5f, -0.5f);
		vertexData[13].Normal = glm::vec3(-1.0f, 0.0f, 0.0f);
		vertexData[13].UV = UVFromBlock(1, 2, 0, 1);
		vertexData[14].Position = glm::vec3(-0.5f, 0.5f, 0.5f);
		vertexData[14].Normal = glm::vec3(-1.0f, 0.0f, 0.0f);
		vertexData[14].UV = UVFromBlock(1, 2, 1, 1);
		vertexData[15].Position = glm::vec3(-0.5f, -0.5f, 0.5f);
		vertexData[15].Normal = glm::vec3(-1.0f, 0.0f, 0.0f);
		vertexData[15].UV = UVFromBlock(1, 2, 1, 0);

		// Top face (0,1,0)
		vertexData[16].Position = glm::vec3(-0.5f, 0.5f, 0.5f);
		vertexData[16].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
		vertexData[16].UV = UVFromBlock(1, 3, 0, 1);
		vertexData[17].Position = glm::vec3(-0.5f, 0.5f, -0.5f);
		vertexData[17].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
		vertexData[17].UV = UVFromBlock(1, 3, 0, 0);
		vertexData[18].Position = glm::vec3(0.5f, 0.5f, -0.5f);
		vertexData[18].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
		vertexData[18].UV = UVFromBlock(1, 3, 1, 0);
		vertexData[19].Position = glm::vec3(0.5f, 0.5f, 0.5f);
		vertexData[19].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
		vertexData[19].UV = UVFromBlock(1, 3, 1, 1);

		// Bottom face (0,-1,0)
		vertexData[20].Position = glm::vec3(-0.5f, -0.5f, -0.5f);
		vertexData[20].Normal = glm::vec3(0.0f, -1.0f, 0.0f);
		vertexData[20].UV = UVFromBlock(1, 1, 0, 1);
		vertexData[21].Position = glm::vec3(-0.5f, -0.5f, 0.5f);
		vertexData[21].Normal = glm::vec3(0.0f, -1.0f, 0.0f);
		vertexData[21].UV = UVFromBlock(1, 1, 0, 0);
		vertexData[22].Position = glm::vec3(0.5f, -0.5f, 0.5f);
		vertexData[22].Normal = glm::vec3(0.0f, -1.0f, 0.0f);
		vertexData[22].UV = UVFromBlock(1, 1, 1, 0);
		vertexData[23].Position = glm::vec3(0.5f, -0.5f, -0.5f);
		vertexData[23].Normal = glm::vec3(0.0f, -1.0f, 0.0f);
		vertexData[23].UV = UVFromBlock(1, 1, 1, 1);

		std::array<uint32_t, 36> indices;
		uint32_t offset = 0;
		for (int i = 0; i < 36; i += 6)
		{
			indices[i + 0] = 0 + offset;
			indices[i + 1] = 1 + offset;
			indices[i + 2] = 2 + offset;
			indices[i + 3] = 2 + offset;
			indices[i + 4] = 3 + offset;
			indices[i + 5] = 0 + offset;

			offset += 4;
		}

		m_VertexBuffer.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		CreateOrResizeBuffer(m_VertexBuffer, vertexData.size() * sizeof(Vertex));
		m_IndexBuffer.Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		CreateOrResizeBuffer(m_IndexBuffer, indices.size() * sizeof(uint32_t));

		glm::vec3* vbMemory;
		VK_CHECK(vkMapMemory(device, m_VertexBuffer.Memory, 0, vertexData.size() * sizeof(Vertex), 0, (void**)&vbMemory));
		memcpy(vbMemory, vertexData.data(), vertexData.size() * sizeof(Vertex));

		uint32_t* ibMemory;
		VK_CHECK(vkMapMemory(device, m_IndexBuffer.Memory, 0, indices.size() * sizeof(uint32_t), 0, (void**)&ibMemory));
		memcpy(ibMemory, indices.data(), indices.size() * sizeof(uint32_t));

		VkMappedMemoryRange range[2] = {};
		range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range[0].memory = m_VertexBuffer.Memory;
		range[0].size = VK_WHOLE_SIZE;
		range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range[1].memory = m_IndexBuffer.Memory;
		range[1].size = VK_WHOLE_SIZE;

		VK_CHECK(vkFlushMappedMemoryRanges(device, 2, range));
		vkUnmapMemory(device, m_VertexBuffer.Memory);
		vkUnmapMemory(device, m_IndexBuffer.Memory);
	}

	VkShaderModule Renderer::loadShader(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);

		if (!stream.is_open() || !stream.good()) {
			WL_ERROR("Failed to open shader file: {}", path.string());
			return nullptr;
		}


		stream.seekg(0, std::ios::end);
		std::streampos size = stream.tellg();
		stream.seekg(0, std::ios::beg);

		std::vector<char> buffer(size);

		stream.read(buffer.data(), size);

		stream.close();

		VkShaderModuleCreateInfo shaderModuleCI = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		shaderModuleCI.pCode = (uint32_t*)buffer.data();
		shaderModuleCI.codeSize = buffer.size();

		VkDevice device = GetVulkanInfo()->Device;
		VkShaderModule result = nullptr;
		VK_CHECK(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &result));
		return result;
	}

	void Renderer::CreateOrResizeBuffer(Buffer& buffer, uint64_t newSize)
	{
		VkDevice device = GetVulkanInfo()->Device;
		if (buffer.Handle != VK_NULL_HANDLE)
			vkDestroyBuffer(device, buffer.Handle, nullptr);
		if (buffer.Memory != VK_NULL_HANDLE)
			vkFreeMemory(device, buffer.Memory, nullptr);

		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = newSize;
		buffer_info.usage = buffer.Usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &buffer.Handle));

		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(device, buffer.Handle, &req);
		//bd->BufferMemoryAlignment = (bd->BufferMemoryAlignment > req.alignment) ? bd->BufferMemoryAlignment : req.alignment;
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = req.size;
		alloc_info.memoryTypeIndex = GetVulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
		VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &buffer.Memory));

		VK_CHECK(vkBindBufferMemory(device, buffer.Handle, buffer.Memory, 0));
		buffer.Size = req.size;
	}
} // namespace Cubed