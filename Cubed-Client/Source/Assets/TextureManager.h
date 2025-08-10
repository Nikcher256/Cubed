#pragma once
#include <filesystem>
#include <unordered_map>
#include <memory>
#include "Texture.h"


namespace Cubed {
	class TextureManager {
	public:
		static uint32_t LoadTexture(const std::filesystem::path& path);
		static std::shared_ptr<Texture> GetTexture(uint32_t textureID);
		static void ClearCache();

	private:
		static std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_TextureCache;
		static uint32_t m_NextTextureID;
	};
}