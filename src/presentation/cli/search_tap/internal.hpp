#pragma once

#include "presentation/cli/search_tap.hpp"

#include <cstdint>

namespace crumb::boundary::detail {
using LocalTime = std::tm* (*)(const std::time_t*);
std::string format_result_date(std::optional<std::int64_t> nanoseconds, LocalTime local_time);
std::uint64_t total_duration(const application::SearchResult& result);
std::string format_duration(std::uint64_t microseconds);
std::string text_escape(std::string_view value);
std::string html_escape(std::string_view value);
std::string render_text(std::string_view query, const domain::DirectoryPath& directory,
                        const application::SearchResult& result);
std::string render_html(std::string_view query, const domain::DirectoryPath& directory,
                        const application::SearchResult& result);
}  // namespace crumb::boundary::detail
