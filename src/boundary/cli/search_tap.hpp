#pragma once

#include "application/search_manifest.hpp"

#include <string>
#include <string_view>

namespace crumb::boundary {

enum class TapFormat { text, html };

[[nodiscard]] std::string render_search_tap(
    std::string_view query, const domain::DirectoryPath& directory,
    const application::SearchResult& result, TapFormat format);

} // namespace crumb::boundary
