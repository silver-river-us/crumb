#pragma once

#include "presentation/cli/command_router.hpp"
#include "presentation/cli/search_tap.hpp"

#include <chrono>
#include <expected>
#include <limits>
#include <string>
#include <vector>

namespace crumb::boundary::detail {
struct SearchOptions {
    std::size_t limit = std::numeric_limits<std::size_t>::max();
    bool full_results = true;
    bool tap_requested = false;
    TapFormat tap_format = TapFormat::text;
    std::vector<std::string_view> positionals;
};

std::expected<SearchOptions, std::string> parse_search_options(int argc, char** argv);
void print_elapsed(std::ostream& output, std::chrono::microseconds elapsed);
std::string shorten(std::string_view value, std::size_t width);
std::string relative_result_path(const domain::DirectoryPath& root,
                                 const application::SearchMatch& match);
std::string clickable_url(std::string_view url, bool interactive, std::string_view label = {});
std::string format_date(const std::optional<std::int64_t>& timestamp_ns);
void print_search_table(std::ostream& output, const domain::DirectoryPath& directory,
                        const application::SearchResult& result, bool scrollable);
void print_search_details(std::ostream& output, const domain::DirectoryPath& directory,
                          const application::SearchResult& result);
}  // namespace crumb::boundary::detail
