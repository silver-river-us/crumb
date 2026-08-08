#pragma once

#include "domain/value_objects/value_objects.hpp"
#include "lib/ports/filesystem.hpp"
#include "lib/ports/manifest_repository.hpp"
#include "lib/ports/search_index_repository.hpp"

#include <expected>
#include <string>

namespace crumb::application {

class RebuildSearchIndex {
public:
    RebuildSearchIndex(ports::ManifestRepository& manifests, ports::FileSystem& filesystem,
                       ports::SearchIndexRepository& index)
        : manifests_(manifests), filesystem_(filesystem), index_(index) {}

    [[nodiscard]] std::expected<void, std::string> execute(const domain::DirectoryPath& directory);

private:
    ports::ManifestRepository& manifests_;
    ports::FileSystem& filesystem_;
    ports::SearchIndexRepository& index_;
};

} // namespace crumb::application
