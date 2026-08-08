#include "application/search_manifest.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace crumb::application {
namespace {
using Location = std::pair<std::string, std::string>;
using MetadataByLocation = std::map<Location, SearchMatch>;

SearchResult to_result(const domain::SearchIndex& index, const domain::SearchQuery& query,
                       std::size_t limit, const MetadataByLocation& metadata = {}) {
    SearchResult result;
    result.inspected = index.documents.size();
    for (const auto& hit : index.search(query)) {
        const auto& location = index.documents[hit.document_id];
        const auto key = Location{location.directory.value(), location.name.value()};
        if (const auto found = metadata.find(key); found != metadata.end()) {
            auto match = found->second;
            match.score = hit.score;
            result.matches.push_back(std::move(match));
        } else {
            result.matches.push_back({location.directory, location.name, hit.score, {}, std::nullopt, std::nullopt});
        }
    }
    std::ranges::sort(result.matches, [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        if (left.directory.value() != right.directory.value()) return left.directory.value() < right.directory.value();
        return left.name.value() < right.name.value();
    });
    if (result.matches.size() > limit) result.matches.resize(limit);
    return result;
}
}

std::expected<SearchResult, std::string> SearchManifest::execute(
    const domain::DirectoryPath& directory, std::string_view query, std::size_t limit) const {
    auto search_query = domain::SearchQuery::create(query);
    if (!search_query) return std::unexpected(search_query.error());

    // The persisted index is the fast path. A missing index deliberately falls back
    // to the manifest path so the library remains useful before the first scan.
    if (index_) {
        auto loaded_index = index_->load(directory);
        if (loaded_index) {
            return to_result(*loaded_index, *search_query, limit);
        }
    }

    auto directories = filesystem_.list_directories_recursive(directory);
    if (!directories) return std::unexpected(directories.error());
    domain::SearchIndexBuilder builder;
    MetadataByLocation metadata;
    for (const auto& current : directories.value()) {
        auto loaded = manifests_.load(current);
        if (!loaded) return std::unexpected(loaded.error());
        if (!loaded.value()) continue;
        for (const auto& entry : loaded.value()->files()) {
            builder.add(current, entry);
            metadata.emplace(Location{current.value(), entry.name.value()},
                             SearchMatch{current, entry.name, 0.0, entry.metadata.type,
                                         entry.metadata.title, entry.metadata.author});
        }
    }
    return to_result(std::move(builder).build(), *search_query, limit, metadata);
}
}
