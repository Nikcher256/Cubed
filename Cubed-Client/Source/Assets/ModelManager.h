#pragma once
#include <filesystem>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>
#include <glm/glm.hpp>
#include "Model.h"

namespace Cubed {

    class ModelManager {
    public:
        static std::shared_ptr<Model> Load(const std::filesystem::path& path, uint32_t id);
        static std::shared_ptr<Model> LoadInstance(const std::filesystem::path& path, const glm::mat4& transform);
        static bool Has(const std::filesystem::path& path);
        static void Remove(const std::filesystem::path& path);
        static void Clear();
        static void GarbageCollect();
        static void RemoveModel(uint32_t id);
        static void ForEachLoaded(const std::function<void(const std::shared_ptr<Model>&)>& fn);

    private:
        static std::filesystem::path Normalize(const std::filesystem::path& p);

    private:
        static std::unordered_map<std::filesystem::path, std::weak_ptr<Model>> s_Cache;
        static std::mutex s_Mutex;
    };

}
