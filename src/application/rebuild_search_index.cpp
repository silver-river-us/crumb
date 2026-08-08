#include "application/rebuild_search_index.hpp"

namespace crumb::application {
std::expected<void, std::string> RebuildSearchIndex::execute(const domain::DirectoryPath& directory) {
    auto directories = filesystem_.list_directories_recursive(directory);
    if (!directories) return std::unexpected(directories.error());

    domain::SearchIndexBuilder builder;

    for (const auto& current : directories.value()) {
        auto loaded = manifests_.load(current);
        if (!loaded) return std::unexpected(loaded.error());
        if (!loaded.value()) continue;

        for (const auto& entry : loaded.value()->files()) builder.add(current, entry);
    }
    return index_.save(directory, std::move(builder).build());
}

} // namespace crumb::application
