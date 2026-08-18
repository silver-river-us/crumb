#include "infrastructure/filesystem/native_filesystem.hpp"

#include "domain/search_index/query.hpp"

#include <exception>
#include <fstream>
#include <iterator>

namespace crumb::infrastructure {
std::expected<domain::FileMetadata, std::string> NativeFileSystem::extract(
    const domain::DirectoryPath& directory, const domain::FileName& name,
    domain::FileMetadata base) {
    const auto dot = name.value().rfind('.');
    base.title = name.value().substr(0, dot);
    auto content = read_text_file(directory, name);
    if (content && content.value()) {
        auto terms = domain::SearchQuery::tokenize(*content.value());
        std::ranges::sort(terms);
        std::string indexed = "|";
        for (const auto& term : terms) indexed += term + "|";
        base.extension_fields["crumb.search_terms_v3"] = std::move(indexed);
    }
    return base;
}

std::expected<std::optional<std::string>, std::string> NativeFileSystem::read_text_file(
    const domain::DirectoryPath& directory, const domain::FileName& name) {
    constexpr std::uintmax_t max_text_file_size = 8ULL * 1024 * 1024;
    try {
        const auto path = std::filesystem::path(directory.value()) / name.value();
        if (!std::filesystem::is_regular_file(path) ||
            std::filesystem::file_size(path) > max_text_file_size)
            return std::nullopt;
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::unexpected("cannot read " + path.string());
        std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
        if (content.find('\0') != std::string::npos) return std::nullopt;
        return content;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
}  // namespace crumb::infrastructure
