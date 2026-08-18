#include "boundary/cli/internal/command_router_internal.hpp"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unistd.h>

namespace crumb::boundary::detail {
void print_elapsed(std::ostream& output, std::chrono::microseconds elapsed) {
    const auto microseconds = elapsed.count();
    if (microseconds < 1'000)
        output << " elapsed_µs=" << microseconds;
    else if (microseconds < 1'000'000)
        output << std::fixed << std::setprecision(3)
               << " elapsed_ms=" << static_cast<double>(microseconds) / 1'000.0;
    else
        output << std::fixed << std::setprecision(6)
               << " elapsed_s=" << static_cast<double>(microseconds) / 1'000'000.0;
}

std::string shorten(std::string_view value, std::size_t width) {
    if (value.size() <= width) return std::string(value);
    if (width <= 3) return std::string(value.substr(0, width));
    const auto suffix = (width - 3) / 2;
    return std::string(value.substr(0, width - 3 - suffix)) + "..." +
           std::string(value.substr(value.size() - suffix));
}

std::string relative_result_path(const domain::DirectoryPath& root,
                                 const application::SearchMatch& match) {
    std::error_code error;
    const auto full = std::filesystem::path(match.directory.value()) / match.name.value();
    const bool invalid = root.value().find('\0') != std::string::npos ||
                         match.directory.value().find('\0') != std::string::npos ||
                         match.name.value().find('\0') != std::string::npos;
    const auto root_path = std::filesystem::absolute(root.value(), error);
    const auto full_path = std::filesystem::absolute(full, error);
    if (invalid) error = std::make_error_code(std::errc::invalid_argument);
    if (error) return full.string();
    const auto relative = std::filesystem::relative(full_path, root_path, error);
    if (error || relative.empty() || relative.string().starts_with("../"))
        return full_path.string();
    return relative.string();
}

std::string clickable_url(std::string_view url, bool interactive, std::string_view label) {
    if (!interactive) return std::string(url);
    const auto visible = label.empty() ? url : label;
    return "\033]8;;" + std::string(url) + "\a" + std::string(visible) + "\033]8;;\a";
}

std::string format_date(const std::optional<std::int64_t>& timestamp_ns) {
    if (!timestamp_ns) return "-";
    const auto seconds = static_cast<std::time_t>(*timestamp_ns / 1'000'000'000);
    const auto* local = std::localtime(&seconds);
    char formatted[11]{};
    if (!local || std::strftime(formatted, sizeof formatted, "%Y-%m-%d", local) == 0) return "-";
    return formatted;
}

void print_search_details(std::ostream& output, const domain::DirectoryPath& directory,
                          const application::SearchResult& result) {
    const bool interactive = ::isatty(STDOUT_FILENO) == 1;
    output << "Search results: " << result.matches.size() << " matches in " << directory.value()
           << '\n';
    for (std::size_t index = 0; index < result.matches.size(); ++index) {
        const auto& match = result.matches[index];
        const auto path = std::filesystem::path(match.directory.value()) / match.name.value();
        output << '\n'
               << '[' << index + 1 << "] " << match.name.value() << '\n'
               << "  id:     " << match.file_id.value_or("-") << '\n'
               << "  path:   " << path.string() << '\n'
               << "  folder: " << path.parent_path().string() << '\n'
               << "  score:  " << std::fixed << std::setprecision(4) << match.score << '\n'
               << "  type:   " << (match.type.empty() ? path.extension().string() : match.type)
               << '\n'
               << "  created: " << format_date(match.created_ns) << '\n'
               << "  edited:  " << format_date(match.modified_ns) << '\n';
        if (match.title) output << "  title:  " << *match.title << '\n';
        if (match.author) output << "  author: " << *match.author << '\n';
        if (match.external_url)
            output << "  url:    " << clickable_url(*match.external_url, interactive) << '\n';
    }
}
}  // namespace crumb::boundary::detail
