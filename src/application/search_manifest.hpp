#pragma once

#include "domain/value_objects/value_objects.hpp"
#include "lib/ports/filesystem.hpp"
#include "lib/ports/manifest_repository.hpp"
#include "lib/ports/search_index_repository.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace crumb::application {
struct SearchMatch {
    domain::DirectoryPath directory;
    domain::FileName name;
    double score{};
    std::string type;
    std::optional<std::string> external_url;
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::optional<std::int64_t> created_ns;
    std::optional<std::int64_t> modified_ns;
    std::optional<std::string> file_id;
};

struct SearchTraceSpan {
    std::string name;
    std::uint64_t offset_us{};
    std::uint64_t duration_us{};
    std::string detail;
};

struct SearchResult {
    std::size_t inspected{};
    std::vector<SearchMatch> matches;
    std::vector<SearchTraceSpan> trace;
};

class SearchManifest {
public:
    SearchManifest(ports::ManifestRepository& manifests, ports::FileSystem& filesystem,
                   ports::SearchIndexRepository* index = nullptr)
        : manifests_(manifests), filesystem_(filesystem), index_(index) {}

    std::expected<SearchResult, std::string> execute(
        const domain::DirectoryPath& directory, std::string_view query,
        std::size_t limit = std::numeric_limits<std::size_t>::max()) const;

private:
    ports::ManifestRepository& manifests_;
    ports::FileSystem& filesystem_;
    ports::SearchIndexRepository* index_{};
};
}
