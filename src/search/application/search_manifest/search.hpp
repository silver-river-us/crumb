#pragma once

#include "search/application/search_manifest/result.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/application/filesystem.hpp"
#include "manifests/application/manifest_repository.hpp"
#include "search/application/search_index_repository.hpp"

#include <cstddef>
#include <expected>
#include <limits>
#include <string>
#include <string_view>

namespace crumb::application {

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

}  // namespace crumb::application
