#include "Model.h"

#include "Walnut/Application.h"
#include "Walnut/Core/Log.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

#include <filesystem>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/component_wise.hpp>

// If you have stb in Walnut vendor (same as Renderer used), include the writer:
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// ---------- helpers ----------
namespace Cubed
{
    // Change this if you ever move your textures folder.
    static const std::filesystem::path kTextureBase =
        "C:/Users/Asus/Documents/Projects/Cubed/Cubed-Client/Assets/Textures";

    // Toggle dumping embedded textures for debugging
    static constexpr bool kDumpEmbeddedTextures = true;

    static std::filesystem::path GetTempDir()
    {
#if defined(_WIN32)
        char buf[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, buf);
        if (n == 0 || n > MAX_PATH) return std::filesystem::temp_directory_path();
        return std::filesystem::path(buf);
#else
        return std::filesystem::temp_directory_path();
#endif
    }

    static glm::mat4 ConvertMatrix(const aiMatrix4x4& m)
    {
        glm::mat4 out;
        out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
        out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
        out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
        out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
        return out;
    }

    // Dump a compressed embedded blob (PNG/JPG/etc.) to disk for inspection
    static void DumpCompressedEmbedded(const aiTexture* tex, int idx, const std::string& hint)
    {
        if (!kDumpEmbeddedTextures) return;
        try {
            std::filesystem::path outDir = GetTempDir() / "glb_textures";
            std::filesystem::create_directories(outDir);

            std::string ext = hint.empty() ? "bin" : hint;
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            std::filesystem::path outPath = outDir / ("embedded_" + std::to_string(idx) + "." + ext);

            std::ofstream f(outPath, std::ios::binary);
            f.write(reinterpret_cast<const char*>(tex->pcData), (std::streamsize)tex->mWidth);
            WL_INFO("[model] Dumped embedded compressed *{} to {}", idx, outPath.string());
        }
        catch (...) {
            WL_WARN("[model] Failed to dump embedded compressed *{}", idx);
        }
    }

    // Dump an uncompressed BGRA embedded texture to disk as PNG
    static void DumpUncompressedEmbeddedBGRA(const aiTexture* tex, int idx)
    {
        if (!kDumpEmbeddedTextures) return;
        try {
            const int w = tex->mWidth, h = tex->mHeight;
            const uint8_t* bgra = reinterpret_cast<const uint8_t*>(tex->pcData);
            std::vector<uint8_t> rgba((size_t)w * h * 4);
            for (size_t i = 0; i < (size_t)w * h; ++i) {
                rgba[i * 4 + 0] = bgra[i * 4 + 2];
                rgba[i * 4 + 1] = bgra[i * 4 + 1];
                rgba[i * 4 + 2] = bgra[i * 4 + 0];
                rgba[i * 4 + 3] = bgra[i * 4 + 3];
            }

            std::filesystem::path outDir = GetTempDir() / "glb_textures";
            std::filesystem::create_directories(outDir);
            std::filesystem::path outPath = outDir / ("embedded_" + std::to_string(idx) + ".png");

            if (stbi_write_png(outPath.string().c_str(), w, h, 4, rgba.data(), w * 4))
                WL_INFO("[model] Dumped embedded BGRA *{} to {}", idx, outPath.string());
            else
                WL_WARN("[model] stbi_write_png failed for embedded BGRA *{}", idx);
        }
        catch (...) {
            WL_WARN("[model] Failed to dump embedded BGRA *{}", idx);
        }
    }

    static uint32_t LoadEmbeddedOrExternalTexture(
        const aiScene* scene,
        const aiString& texPath,
        const std::filesystem::path& baseDir)
    {
        // ---- Embedded: "*N" ----
        if (texPath.length > 0 && texPath.C_Str()[0] == '*') {
            int idx = std::atoi(texPath.C_Str() + 1);
            if (idx >= 0 && idx < (int)scene->mNumTextures) {
                const aiTexture* tex = scene->mTextures[idx];
                std::string hint = tex->achFormatHint; // "png","jpg","ktx2","dds","webp", ...
                WL_INFO("[model] Embedded tex *{}  hint='{}'  size={}  height={}", idx, hint, tex->mWidth, tex->mHeight);

                if (tex->mHeight == 0) {
                    // Compressed blob (stb_image can do png/jpg/jpeg only)
                    std::string h = hint; std::transform(h.begin(), h.end(), h.begin(), ::tolower);
                    //DumpCompressedEmbedded(tex, idx, h);
                    bool stbOk = (h == "png" || h == "jpg" || h == "jpeg");
                    if (!stbOk) {
                        WL_WARN("[model] Unsupported embedded '{}', using checker (re-export as PNG/JPG)", hint);
                        return TextureManager::GetDefaultChecker();
                    }
                    return TextureManager::LoadTextureFromMemory(tex->pcData, (size_t)tex->mWidth);
                }
                // Uncompressed BGRA8888
                //DumpUncompressedEmbeddedBGRA(tex, idx);
                const int w = tex->mWidth, h = tex->mHeight;
                const uint8_t* bgra = reinterpret_cast<const uint8_t*>(tex->pcData);
                std::vector<uint8_t> rgba((size_t)w * h * 4);
                for (size_t i = 0; i < (size_t)w * h; ++i) {
                    rgba[i * 4 + 0] = bgra[i * 4 + 2];
                    rgba[i * 4 + 1] = bgra[i * 4 + 1];
                    rgba[i * 4 + 2] = bgra[i * 4 + 0];
                    rgba[i * 4 + 3] = bgra[i * 4 + 3];
                }
                return TextureManager::CreateFromRawRGBA(w, h, rgba.data());
            }
            WL_WARN("[model] Embedded reference '{}' out of range; using checker", texPath.C_Str());
            return TextureManager::GetDefaultChecker();
        }

        // ---- External: remap by filename to your texture folder ----
        std::filesystem::path original = texPath.C_Str();
        std::filesystem::path filename = original.filename(); // name.ext
        std::filesystem::path remapped = kTextureBase / filename;

        WL_INFO("[model] External tex request '{}', trying '{}'", original.string(), remapped.string());

        if (std::filesystem::exists(remapped)) {
            return TextureManager::LoadTexture(remapped);
        }

        // Optional: case-insensitive fallback search in textureBase
        try {
            std::string targetLower = filename.string();
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

            for (auto& entry : std::filesystem::directory_iterator(kTextureBase)) {
                if (!entry.is_regular_file()) continue;
                std::string candLower = entry.path().filename().string();
                std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);
                if (candLower == targetLower) {
                    WL_INFO("[model] Case-insensitive texture match: {}", entry.path().string());
                    return TextureManager::LoadTexture(entry.path());
                }
            }
        }
        catch (...) {}

        WL_WARN("[model] Texture not found (remapped): {}", remapped.string());
        return TextureManager::GetDefaultChecker();
    }

    // Host-visible buffer creation (mirrors your Renderer utility)
    static void CreateOrResizeBuffer(Buffer& buffer, uint64_t newSize)
    {
        VkDevice device = GetVulkanInfo()->Device;
        if (buffer.Handle)  vkDestroyBuffer(device, buffer.Handle, nullptr);
        if (buffer.Memory)  vkFreeMemory(device, buffer.Memory, nullptr);

        VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bi.size = newSize;
        bi.usage = buffer.Usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &buffer.Handle));

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, buffer.Handle, &req);

        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = GetVulkanMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &buffer.Memory));
        VK_CHECK(vkBindBufferMemory(device, buffer.Handle, buffer.Memory, 0));

        buffer.Size = req.size;
    }

} // namespace Cubed

// ---------------------------------------------

namespace Cubed
{
    Model::Model(const std::filesystem::path& path)
    {
        LoadModel(path);
    }

    void Model::LoadModel(const std::filesystem::path& path)
    {
        Assimp::Importer importer;

        // Pick postprocess flags depending on extension:
        unsigned int pp =
            aiProcess_Triangulate |
            aiProcess_GenNormals;          // let Assimp build normals if missing

        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".gltf" && ext != ".glb")
        {
            // Many formats (FBX/OBJ) need V flipped, but glTF already matches OpenGL.
            pp |= aiProcess_FlipUVs;
        }

        const aiScene* scene = importer.ReadFile(path.string(), pp);
        if (!scene || !scene->mRootNode)
        {
            throw std::runtime_error("Failed to load model: " + path.string());
        }

        const std::filesystem::path modelDir = path.parent_path();

        std::function<void(aiNode*, const aiScene*, glm::mat4)> processNode;
        processNode = [&](aiNode* node, const aiScene* scn, glm::mat4 parentTransform)
            {
                glm::mat4 globalTransform = parentTransform * ConvertMatrix(node->mTransformation);

                for (unsigned int i = 0; i < node->mNumMeshes; ++i)
                {
                    aiMesh* mesh = scn->mMeshes[node->mMeshes[i]];
                    ProcessMesh(mesh, scn, modelDir, globalTransform);
                }
                for (unsigned int i = 0; i < node->mNumChildren; ++i)
                    processNode(node->mChildren[i], scn, globalTransform);
            };

        m_Meshes.clear();
        processNode(scene->mRootNode, scene, glm::mat4(1.0f));

        // Optional summary
        WL_INFO("[Model Info] Mesh count: {}", (int)m_Meshes.size());
        for (size_t i = 0; i < m_Meshes.size(); ++i)
        {
            WL_INFO("  Mesh({}){} - TextureIndex: {} (texture loaded)",
                m_Meshes[i].Name, (int)i, m_Meshes[i].TextureIndex);
        }
    }

    // Uniform scale so that the model’s largest dimension = meters.
    void Model::SetSizeMeters(float meters)
    {
        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        for (const auto& mesh : m_Meshes)
        {
            for (const auto& v : mesh.Vertices)
            {
                minBounds = glm::min(minBounds, v.Position);
                maxBounds = glm::max(maxBounds, v.Position);
            }
        }

        glm::vec3 size = maxBounds - minBounds;
        float maxDim = glm::compMax(size);
        if (maxDim <= 1e-6f) return;

        float s = meters / maxDim;

        // preserve current rotation/translation; replace scale
        // Decompose current transform roughly: we’ll just rebuild as S * R * T with same R,T.
        // Since we don’t store R/T separately, simplest is: new = scale * old (uniform scale ok).
        m_Transform = glm::scale(glm::mat4(1.0f), glm::vec3(s)) * m_Transform;
    }

    void Model::SetRotation(float degrees, const glm::vec3& axis)
    {
        m_Transform = glm::rotate(glm::mat4(1.0f), glm::radians(degrees), axis) * m_Transform;
    }

    void Model::SetPosition(const glm::vec3& position)
    {
        // Keep current rotation/scale, just set translation column
        m_Transform[3] = glm::vec4(position, 1.0f);
    }

    void Model::ProcessMesh(aiMesh* mesh,
        const aiScene* scene,
        const std::filesystem::path& modelDir,
        const glm::mat4& nodeTransform)
    {
        std::filesystem::path basePath = modelDir;

        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(mesh->mNumVertices);

        // Inverse-transpose for normals
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));

        // ---- choose UV channel (first with >=2 components) ----
        int uvChannel = -1;
        for (int ch = 0; ch < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++ch) {
            if (mesh->mTextureCoords[ch] && mesh->mNumUVComponents[ch] >= 2) {
                uvChannel = ch;
                break;
            }
        }
        WL_INFO("[model] Mesh '{}' using UV channel {}", mesh->mName.C_Str(), uvChannel);

        // ---- build vertices ----
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            glm::vec4 p = nodeTransform *
                glm::vec4(mesh->mVertices[i].x,
                    mesh->mVertices[i].y,
                    mesh->mVertices[i].z, 1.0f);

            glm::vec3 n(0.0f);
            if (mesh->HasNormals()) {
                n = normalMat * glm::vec3(mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z);
                n = glm::normalize(n);
            }

            Vertex v{};
            v.Position = glm::vec3(p);
            v.Normal = n;

            if (uvChannel >= 0) {
                v.UV = {
                    mesh->mTextureCoords[uvChannel][i].x,
                    mesh->mTextureCoords[uvChannel][i].y
                };
            }
            else {
                v.UV = glm::vec2(0.0f);
            }

            vertices.push_back(v);
        }

        // ---- indices ----
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& f = mesh->mFaces[i];
            for (unsigned int j = 0; j < f.mNumIndices; ++j)
                indices.push_back(f.mIndices[j]);
        }

        // ---- texture resolve (base/diffuse first, then fallbacks) ----
        uint32_t texIndex = TextureManager::GetDefaultChecker();

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

            auto trySlot = [&](aiTextureType type, const char* label) -> uint32_t {
                if (mat->GetTextureCount(type) > 0) {
                    aiString path;
                    unsigned int uvIndex = 0; // which UV set the material expects
                    if (mat->GetTexture(type, 0, &path, nullptr, &uvIndex, nullptr, nullptr) == aiReturn_SUCCESS) {
                        WL_INFO("[model] Material '{}' slot {} -> '{}' (uvIndex={})",
                            mat->GetName().C_Str(), label, path.C_Str(), uvIndex);
                        return LoadEmbeddedOrExternalTexture(scene, path, basePath);
                    }
                }
                return UINT32_MAX;
                };

            uint32_t found = UINT32_MAX;
            if (found == UINT32_MAX) found = trySlot(aiTextureType_BASE_COLOR, "BASE_COLOR");
            if (found == UINT32_MAX) found = trySlot(aiTextureType_DIFFUSE, "DIFFUSE");
            if (found == UINT32_MAX) found = trySlot(aiTextureType_UNKNOWN, "UNKNOWN");
            if (found == UINT32_MAX) found = trySlot(aiTextureType_SPECULAR, "SPECULAR");
            if (found == UINT32_MAX) found = trySlot(aiTextureType_EMISSIVE, "EMISSIVE");
            if (found == UINT32_MAX) found = trySlot(aiTextureType_AMBIENT, "AMBIENT");

            if (found != UINT32_MAX) {
                texIndex = found;
            }
            else {
                // solid from diffuse color as fallback
                aiColor4D col{};
                if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &col)) {
                    auto to8 = [](float f) -> uint8_t {
                        return (uint8_t)std::roundf(std::clamp(f, 0.0f, 1.0f) * 255.0f);
                        };
                    texIndex = TextureManager::CreateSolid(to8(col.r), to8(col.g), to8(col.b),
                        to8(col.a ? col.a : 1.0f));
                    WL_INFO("[model] No texture found; using solid color ({:.2f},{:.2f},{:.2f},{:.2f})",
                        col.r, col.g, col.b, col.a);
                }
                else {
                    WL_INFO("[model] No texture and no diffuse color; using checker");
                }
            }
        }

        // ---- pack mesh ----
        Mesh out{};
        out.Name = mesh->mName.C_Str();
        out.IndexCount = (uint32_t)indices.size();
        out.TextureIndex = texIndex;
        out.Vertices = vertices; // keep CPU copy for bounds/meters

        // ---- GPU buffers ----
        out.VertexBuffer.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        CreateOrResizeBuffer(out.VertexBuffer, vertices.size() * sizeof(Vertex));

        out.IndexBuffer.Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        CreateOrResizeBuffer(out.IndexBuffer, indices.size() * sizeof(uint32_t));

        // Upload data
        {
            void* vbMem = nullptr;
            vkMapMemory(GetVulkanInfo()->Device, out.VertexBuffer.Memory, 0,
                vertices.size() * sizeof(Vertex), 0, &vbMem);
            std::memcpy(vbMem, vertices.data(), vertices.size() * sizeof(Vertex));
            vkUnmapMemory(GetVulkanInfo()->Device, out.VertexBuffer.Memory);
        }
        {
            void* ibMem = nullptr;
            vkMapMemory(GetVulkanInfo()->Device, out.IndexBuffer.Memory, 0,
                indices.size() * sizeof(uint32_t), 0, &ibMem);
            std::memcpy(ibMem, indices.data(), indices.size() * sizeof(uint32_t));
            vkUnmapMemory(GetVulkanInfo()->Device, out.IndexBuffer.Memory);
        }

        m_Meshes.push_back(std::move(out));
    }


    uint32_t Model::LoadMaterialTexture(aiMaterial* mat,
        aiTextureType type,
        const aiScene* scene,
        const std::filesystem::path& baseDir)
    {
        if (!mat) return TextureManager::GetDefaultChecker();

        if (mat->GetTextureCount(type) > 0)
        {
            aiString str;
            if (mat->GetTexture(type, 0, &str) == aiReturn_SUCCESS)
                return LoadEmbeddedOrExternalTexture(scene, str, baseDir);
        }
        return TextureManager::GetDefaultChecker();
    }

    void Model::UploadToGPU() { /* already uploaded per-mesh */ }

    void Model::DestroyGPU()
    {
        VkDevice device = GetVulkanInfo()->Device;
        for (auto& m : m_Meshes)
        {
            if (m.VertexBuffer.Handle) vkDestroyBuffer(device, m.VertexBuffer.Handle, nullptr);
            if (m.VertexBuffer.Memory) vkFreeMemory(device, m.VertexBuffer.Memory, nullptr);
            if (m.IndexBuffer.Handle)  vkDestroyBuffer(device, m.IndexBuffer.Handle, nullptr);
            if (m.IndexBuffer.Memory)  vkFreeMemory(device, m.IndexBuffer.Memory, nullptr);
        }
        m_Meshes.clear();
    }

} // namespace Cubed
