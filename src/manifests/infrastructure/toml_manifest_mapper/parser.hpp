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

namespace crumb::infrastructure::detail {
std::expected<domain::DirectoryManifest, std::string> parse_manifest(
    std::string_view input, const domain::DirectoryPath& path);
}  // namespace crumb::infrastructure::detail
