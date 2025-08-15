#include "TextureManager.h"
#include "../../../Walnut/vendor/stb_image/stb_image.h"

namespace Cubed {

	std::unordered_map<uint32_t, std::shared_ptr<Cubed::Texture>> TextureManager::m_TextureCache;
	uint32_t TextureManager::m_NextTextureID = 0;
	uint32_t TextureManager::s_DefaultWhite = 0xffffffffu;
	uint32_t TextureManager::s_DefaultChecker = 0xffffffffu;

	void TextureManager::EnsureDefaults() {
		if (m_TextureCache.find(s_DefaultWhite) != m_TextureCache.end())
			return;

		// 1x1 white
		uint8_t wpx[4] = { 255,255,255,255 };
		auto white = std::make_shared<Texture>(1, 1, Walnut::Buffer(wpx, 4));
		s_DefaultWhite = m_NextTextureID++;
		m_TextureCache[s_DefaultWhite] = white;

		// 2x2 checker (black/gray)
		uint8_t chk[16] = {
			30,30,30,255,  180,180,180,255,
			180,180,180,255, 30,30,30,255
		};
		auto checker = std::make_shared<Texture>(2, 2, Walnut::Buffer(chk, 16));
		s_DefaultChecker = m_NextTextureID++;
		m_TextureCache[s_DefaultChecker] = checker;
	}

	uint32_t TextureManager::CreateSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		EnsureDefaults();
		uint8_t px[4] = { r,g,b,a };
		auto tex = std::make_shared<Texture>(1, 1, Walnut::Buffer(px, 4));
		uint32_t id = m_NextTextureID++;
		m_TextureCache[id] = tex;
		return id;
	}

	uint32_t TextureManager::GetDefaultWhite() { EnsureDefaults(); return s_DefaultWhite; }
	uint32_t TextureManager::GetDefaultChecker() { EnsureDefaults(); return s_DefaultChecker; }

	uint32_t TextureManager::CreateFromRawRGBA(int width, int height, const void* rgba) {
		// Directly create a Texture from raw RGBA8 pixels (no stbi)
		auto tex = std::make_shared<Texture>(width, height, Walnut::Buffer((void*)rgba, (size_t)width * height * 4));
		uint32_t id = m_NextTextureID++;
		m_TextureCache[id] = tex;
		return id;
	}

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

	uint32_t TextureManager::LoadTextureFromMemory(const void* data, size_t sizeBytes) {
		int w, h, channels;
		stbi_uc* pixels = stbi_load_from_memory((const stbi_uc*)data, (int)sizeBytes, &w, &h, &channels, STBI_rgb_alpha);
		if (!pixels) throw std::runtime_error("Failed to load texture from memory");
		auto tex = std::make_shared<Texture>(w, h, Walnut::Buffer(pixels, (size_t)w * h * 4));
		stbi_image_free(pixels);
		uint32_t id = m_NextTextureID++;
		m_TextureCache[id] = tex;
		return id;
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