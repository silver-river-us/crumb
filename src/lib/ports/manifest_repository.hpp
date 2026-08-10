#pragma once

#include "domain/directory_manifest.hpp"
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace crumb::ports {
using ManifestError = std::string;
using LoadedManifest = std::optional<domain::DirectoryManifest>;
using LoadedManifestBatch = std::vector<std::pair<domain::DirectoryPath, LoadedManifest>>;

class ManifestRepository {
public:
    virtual ~ManifestRepository() = default;
    virtual std::expected<LoadedManifest, ManifestError>
    load(const domain::DirectoryPath& directory) = 0;

    virtual std::expected<LoadedManifestBatch, ManifestError> load_many(
        const std::vector<domain::DirectoryPath>& directories) {
        LoadedManifestBatch result;
        result.reserve(directories.size());
        for (const auto& directory : directories) {
            auto loaded = load(directory);
            if (!loaded) return std::unexpected(loaded.error());
            result.emplace_back(directory, std::move(loaded.value()));
        }
        return result;
    }

    virtual std::expected<void, ManifestError> save(const domain::DirectoryManifest& manifest) = 0;
};
}
