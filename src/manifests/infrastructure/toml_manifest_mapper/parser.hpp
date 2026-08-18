#pragma once

#include "manifests/domain/directory_manifest.hpp"

#include <expected>
#include <string_view>

namespace crumb::infrastructure::detail {
std::expected<domain::DirectoryManifest, std::string> parse_manifest(
    std::string_view input, const domain::DirectoryPath& path);
}  // namespace crumb::infrastructure::detail
