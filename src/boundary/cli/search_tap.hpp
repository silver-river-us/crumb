#pragma once

#include "application/search_manifest.hpp"

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace crumb::boundary {

enum class TapFormat { text, html };

std::string format_result_date_for_test(
    std::optional<std::int64_t> nanoseconds,
    std::tm* (*local_time)(const std::time_t*) = std::localtime);

[[nodiscard]] std::string render_search_tap(std::string_view query,
                                            const domain::DirectoryPath& directory,
                                            const application::SearchResult& result,
                                            TapFormat format);

}  // namespace crumb::boundary
