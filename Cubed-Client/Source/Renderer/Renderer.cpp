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

namespace Cubed {

	uint32_t Renderer::GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
	{
		VkPhysicalDevice physicalDevice = Cubed::GetVulkanInfo()->PhysicalDevice;
		VkPhysicalDeviceMemoryProperties prop;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
		for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
			if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
				return i;
		return 0xFFFFFFFF; // Unable to find memoryType
	}

	void Renderer::Init()
	{
		const std::filesystem::path TEXTURE_BASE_PATH = "C:/Users/Asus/Documents/Projects/Cubed/Cubed-Client/Assets/Textures";
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load((TEXTURE_BASE_PATH / "simple.png").string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error("Failed to load texture image!");
		}

		uint32_t color = 0xFFFF00FF;
		m_Texture = std::make_shared<Texture>(
			texWidth,
			texHeight,
			Walnut::Buffer(pixels, texWidth * texHeight * 4) // 4 bytes per pixel
		);

		stbi_image_free(pixels);

		VkDevice device = GetVulkanInfo()->Device;

		//VkSampler sampler[1] = { bd->FontSampler };
		VkDescriptorSetLayoutBinding binding[1] = {};
		binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding[0].descriptorCount = 1;
		binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		//binding[0].pImmutableSamplers = sampler;
		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = 1;
		info.pBindings = binding;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &m_DescriptorSetLayout));

		m_DescriptorSet = Walnut::Application::AllocateDescriptorSet(m_DescriptorSetLayout);
		VkWriteDescriptorSet wds{};
		wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds.descriptorCount = 1;
		wds.dstSet = m_DescriptorSet;
		wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		wds.dstBinding = 0;
		VkDescriptorImageInfo imageInfo = m_Texture->GetImageInfo(); // ensure this returns a valid object
		wds.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

		InitBuffers();
		InitPipeline();
	}

	void Renderer::Shutdown()
	{
		VkDevice device = GetVulkanInfo()->Device;

		// Destroy pipeline
		if (m_GraphicsPipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device, m_GraphicsPipeline, nullptr);

		// Destroy pipeline layout
		if (m_PipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

		// Destroy vertex buffer and free memory
		if (m_VertexBuffer.Handle != VK_NULL_HANDLE)
			vkDestroyBuffer(device, m_VertexBuffer.Handle, nullptr);
		if (m_VertexBuffer.Memory != VK_NULL_HANDLE)
			vkFreeMemory(device, m_VertexBuffer.Memory, nullptr);

		// Destroy index buffer and free memory
		if (m_IndexBuffer.Handle != VK_NULL_HANDLE)
			vkDestroyBuffer(device, m_IndexBuffer.Handle, nullptr);
		if (m_IndexBuffer.Memory != VK_NULL_HANDLE)
			vkFreeMemory(device, m_IndexBuffer.Memory, nullptr);

		// Destroy descriptor set layout
		if (m_DescriptorSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
		if (m_DescriptorSet != VK_NULL_HANDLE)
			vkFreeDescriptorSets(device, Walnut::Application::GetDescriptorPool(), 1, &m_DescriptorSet);

		m_GraphicsPipeline = VK_NULL_HANDLE;
		m_PipelineLayout = VK_NULL_HANDLE;
		m_VertexBuffer = {};
		m_IndexBuffer = {};
	}

	void Renderer::BeginScene(const Camera& camera)
	{
		auto wd = Walnut::Application::GetMainWindowData();

		float viewportWidth = static_cast<float>(wd->Width);
		float viewportHeight = static_cast<float>(wd->Height);

		VkCommandBuffer commandBuffer = Walnut::Application::GetActiveCommandBuffer();

		glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), camera.Position) *
			glm::eulerAngleXYZ(glm::radians(camera.Rotation.x), glm::radians(camera.Rotation.y), glm::radians(camera.Rotation.z));;
		m_PushConstants.ViewProjection = glm::perspectiveFov(glm::radians(45.0f), viewportWidth, viewportHeight, 0.1f, 1000.0f)
			* glm::inverse(cameraTransform);


		VkViewport vp{
			.y = viewportHeight,
			.width = viewportWidth,
			.height = -viewportHeight,
			.minDepth = 0.0f,
			.maxDepth = 1.0f };
		// Set viewport dynamically
		vkCmdSetViewport(commandBuffer, 0, 1, &vp);

		VkRect2D scissor{
			.extent = {.width = (uint32_t)wd->Width, .height = (uint32_t)wd->Height} };
		// Set scissor dynamically
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::RenderCube(const glm::vec3& position, const glm::vec3& rotation)
	{
		glm::vec3 translation = position;

		m_PushConstants.Transform = glm::translate(glm::mat4(1.0f), translation) *
			glm::eulerAngleXYZ(glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z));

		VkCommandBuffer commandBuffer = Walnut::Application::GetActiveCommandBuffer();

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

		vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &m_PushConstants);

		VkDeviceSize offset{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VertexBuffer.Handle, &offset);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.Handle, offset, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, (uint32_t)m_IndexBuffer.Size / 4, 1, 0, 0, 0);
	}

	void Renderer::RenderUI()
	{
		
	}

	void Renderer::InitPipeline()
	{

		VkDevice device = GetVulkanInfo()->Device;

		VkRenderPass renderPass = Walnut::Application::GetMainWindowData()->RenderPass; 
		
		std::array<VkPushConstantRange, 1> pushConstantRanges;
		pushConstantRanges[0].offset = 0;
		pushConstantRanges[0].size = sizeof(glm::mat4) * 2;
		pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		layout_info.pPushConstantRanges = pushConstantRanges.data();
		layout_info.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
		layout_info.setLayoutCount = 1;
		layout_info.pSetLayouts = &m_DescriptorSetLayout;
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
			.cullMode = VK_CULL_MODE_BACK_BIT,
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

		// Disable all depth testing.
		VkPipelineDepthStencilStateCreateInfo depth_stencil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

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
			.renderPass = renderPass             // We need to specify the render pass up front
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