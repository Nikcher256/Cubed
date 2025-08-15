#pragma once
#include <filesystem>
#include <unordered_map>
#include <memory>
#include "Texture.h"


namespace Cubed {
	class TextureManager {
    public:
        static uint32_t LoadTexture(const std::filesystem::path& path);
        static uint32_t LoadTextureFromMemory(const void* data, size_t sizeBytes); // if you added earlier

        // NEW: create tiny solid colors, and get defaults
        static uint32_t CreateSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        static uint32_t GetDefaultWhite();
        static uint32_t GetDefaultChecker();
        static uint32_t CreateFromRawRGBA(int width, int height, const void* rgba); // expects width*height*4 bytes


        static std::shared_ptr<Cubed::Texture> GetTexture(uint32_t textureID);
        static void ClearCache();

        static size_t Count() {
            return m_TextureCache.size();
		}
    private:
        static void EnsureDefaults();
        static std::unordered_map<uint32_t, std::shared_ptr<Cubed::Texture>> m_TextureCache;
        static uint32_t m_NextTextureID;
        static uint32_t s_DefaultWhite, s_DefaultChecker;
	};
}