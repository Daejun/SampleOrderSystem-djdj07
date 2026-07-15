#include "JsonDocument.h"

#include <fstream>
#include <stdexcept>

namespace sampleorder {

namespace {

constexpr const char* kTempSuffix = ".tmp";
constexpr int kIndentWidth = 2;

} // namespace

void JsonDocument::load(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("파일을 열 수 없습니다: " + file.string());
    }

    nlohmann::ordered_json parsed;
    try {
        in >> parsed;
    } catch (const nlohmann::ordered_json::parse_error& e) {
        throw std::runtime_error(std::string("JSON 파싱 오류: ") + e.what());
    }

    root_ = std::move(parsed);
    currentFile_ = file;
}

void JsonDocument::save(const std::filesystem::path& file) const {
    auto tmp = file;
    tmp += kTempSuffix;

    {
        std::ofstream out(tmp);
        if (!out) {
            throw std::runtime_error("임시 파일을 생성할 수 없습니다: " + tmp.string());
        }
        out << root_.dump(kIndentWidth);
    }

    std::filesystem::rename(tmp, file);
}

} // namespace sampleorder
