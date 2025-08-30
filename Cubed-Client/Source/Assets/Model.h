#pragma once

// Assimp must be included BEFORE entering your namespace,
// otherwise types get looked up as Cubed::aiMesh etc.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>

#include <filesystem>
#include <vector>
#include <glm/glm.hpp>

#include "../Renderer/Vulkan.h"                 // Buffer, GetVulkanInfo(), GetVulkanMemoryType(...)
#include "../Assets/TextureManager.h"

namespace Cubed {

    struct Buffer {
        VkBuffer       Handle = nullptr;
        VkDeviceMemory Memory = nullptr;
        VkDeviceSize   Size = 0;
        VkBufferUsageFlagBits Usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
	};

    struct Vertex {
        glm::vec3 Position{};
        glm::vec3 Normal{};
        glm::vec2 UV{};
    };

    struct Mesh {
        Buffer   VertexBuffer{};
        Buffer   IndexBuffer{};
        uint32_t IndexCount = 0;
        uint32_t TextureIndex = 0; // index into TextureManager array
		std::string Name; // Optional name for the mesh, useful for debugging

        std::vector<Vertex> Vertices;
    };

    class Model {
    public:
        explicit Model(const std::filesystem::path& path);
        ~Model() = default;

        void UploadToGPU();   // (no-op; kept for API parity)
        void DestroyGPU();

        const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }

        const glm::mat4& GetTransform() const { return m_Transform; }
        void SetTransform(const glm::mat4& t) { m_Transform = t; }
        void SetSizeMeters(float meters);
        void SetRotation(float degrees, const glm::vec3& axis);
        void SetPosition(const glm::vec3& position);
		void SetID(uint32_t id) { m_ID = id; }
		uint32_t GetID() const { return m_ID; }
    private:
        void LoadModel(const std::filesystem::path& path);
        void ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::filesystem::path& basePath, const glm::mat4& transform);

        // Loads either embedded (*N) or external texture, with color/checker fallbacks.
        uint32_t LoadMaterialTexture(aiMaterial* mat,
            aiTextureType type,
            const aiScene* scene,
            const std::filesystem::path& baseDir);

    private:
        uint32_t m_ID;
        std::vector<Mesh> m_Meshes;
        glm::mat4         m_Transform{ 1.0f };
    };

} // namespace Cubed
