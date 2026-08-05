#pragma once

#include "domain/directory_manifest.hpp"
#include <expected>
#include <optional>
#include <string>

namespace crumb::ports {
using ManifestError = std::string;
class ManifestRepository {
public:
    virtual ~ManifestRepository() = default;
    virtual std::expected<std::optional<domain::DirectoryManifest>, ManifestError>
    load(const domain::DirectoryPath& directory) = 0;
    virtual std::expected<void, ManifestError> save(const domain::DirectoryManifest& manifest) = 0;
};
}
