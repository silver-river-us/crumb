#pragma once
#include <expected>
#include <string_view>
#include <string>

#include "manifests/domain/directory_manifest.hpp"

namespace crumb {
namespace domain {
class DirectoryPath;
}  // namespace domain
}  // namespace crumb

namespace crumb::infrastructure {
class TomlManifestMapper {
   public:
    std::expected<domain::DirectoryManifest, std::string> fromToml(std::string_view input) const;
    std::expected<domain::DirectoryManifest, std::string> fromToml(
        std::string_view input, const domain::DirectoryPath& path) const;
    std::string toToml(const domain::DirectoryManifest& manifest) const;
};
}  // namespace crumb::infrastructure
