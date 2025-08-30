#include "ModelManager.h"

namespace Cubed {

    std::unordered_map<std::filesystem::path, std::weak_ptr<Model>> ModelManager::s_Cache;
    std::mutex ModelManager::s_Mutex;

    std::filesystem::path ModelManager::Normalize(const std::filesystem::path& p) {
        std::error_code ec;
        auto abs = std::filesystem::absolute(p, ec);
        if (ec) abs = p;
        return abs.lexically_normal();
    }

    std::shared_ptr<Model> ModelManager::Load(const std::filesystem::path& path, uint32_t id) {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Model file does not exist: " + path.string());
        }
        const auto key = Normalize(path);
        std::lock_guard<std::mutex> lock(s_Mutex);

        if (auto it = s_Cache.find(key); it != s_Cache.end()) {
            if (auto alive = it->second.lock())
                return alive;
        }

        auto model = std::make_shared<Model>(key);
		model->SetID(id);
        s_Cache[key] = model;
        return model;
    }

    void ModelManager::RemoveModel(uint32_t id) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        for (auto it = s_Cache.begin(); it != s_Cache.end(); ) {
            if (auto m = it->second.lock()) {
                if (m->GetID() == id) {
                    it = s_Cache.erase(it);
                    continue;
                }
            }
            ++it;
        }
	}

    std::shared_ptr<Model> ModelManager::LoadInstance(const std::filesystem::path& path, const glm::mat4& transform) {
        /*auto m = Load(path);-
        m->SetTransform(transform);
        return m;*/
        return nullptr;
    }

    bool ModelManager::Has(const std::filesystem::path& path) {
        const auto key = Normalize(path);
        std::lock_guard<std::mutex> lock(s_Mutex);
        auto it = s_Cache.find(key);
        if (it == s_Cache.end()) return false;
        return !it->second.expired();
    }

    void ModelManager::Remove(const std::filesystem::path& path) {
        const auto key = Normalize(path);
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Cache.erase(key);
    }

    void ModelManager::Clear() {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Cache.clear();
    }

    void ModelManager::GarbageCollect() {
        std::lock_guard<std::mutex> lock(s_Mutex);
        for (auto it = s_Cache.begin(); it != s_Cache.end(); ) {
            if (it->second.expired()) it = s_Cache.erase(it);
            else ++it;
        }
    }

    void ModelManager::ForEachLoaded(const std::function<void(const std::shared_ptr<Model>&)>& fn) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        for (auto& [_, weak] : s_Cache) {
            if (auto m = weak.lock()) fn(m);
        }
    }

}
