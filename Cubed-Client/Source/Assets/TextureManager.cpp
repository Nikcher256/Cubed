#include "TextureManager.h"
#include "../../../Walnut/vendor/stb_image/stb_image.h"

namespace Cubed {

	std::unordered_map<uint32_t, std::shared_ptr<Cubed::Texture>> TextureManager::m_TextureCache;
	uint32_t TextureManager::m_NextTextureID;

	uint32_t TextureManager::LoadTexture(const std::filesystem::path& path)
	{
		int width, height, channels;
		stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error("Failed to load texture image: " + path.string());
		}

		auto texture = std::make_shared<Texture>(
			width,
			height,
			Walnut::Buffer(pixels, (width) * (height) * 4)
		);

		stbi_image_free(pixels);

		uint32_t textureID = m_NextTextureID++;
		m_TextureCache[textureID] = texture;

		return textureID;
	}

	std::shared_ptr<Cubed::Texture> TextureManager::GetTexture(uint32_t textureID)
	{
		return m_TextureCache.at(textureID);
	}

	void TextureManager::ClearCache()
	{
		m_TextureCache.clear();  // This will reduce the reference count to zero for all textures.
	}

}