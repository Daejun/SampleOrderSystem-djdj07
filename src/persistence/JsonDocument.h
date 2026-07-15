#pragma once

#include <filesystem>
#include <functional>

#include <nlohmann/json.hpp>

namespace sampleorder {

class JsonDocument {
public:
    void load(const std::filesystem::path& file);
    void save(const std::filesystem::path& file) const;

    const nlohmann::ordered_json& root() const { return root_; }
    void mutate(const std::function<void(nlohmann::ordered_json&)>& fn) { fn(root_); }

    bool hasFile() const { return !currentFile_.empty(); }
    const std::filesystem::path& currentFile() const { return currentFile_; }

private:
    nlohmann::ordered_json root_;
    std::filesystem::path currentFile_;
};

} // namespace sampleorder
