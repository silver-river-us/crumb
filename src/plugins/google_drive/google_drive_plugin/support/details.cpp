#include "plugins/google_drive/google_drive_plugin/support/details.hpp"

#include "domain/search_index/query.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>

#ifdef __APPLE__
#include <sys/xattr.h>
#endif

namespace crumb::plugins::google_drive::detail {
std::int64_t file_time_ns(std::filesystem::file_time_type time) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

std::string mime_type(const std::string& name) {
    const auto dot = name.rfind('.');
    const auto extension = dot == std::string::npos ? "" : name.substr(dot + 1);
    if (extension == "pdf") return "application/pdf";
    if (extension == "md") return "text/markdown";
    if (extension == "txt") return "text/plain";
    if (extension == "doc") return "application/msword";
    if (extension == "docx")
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (extension == "rtf") return "application/rtf";
    if (extension == "html" || extension == "htm") return "text/html";
    return "application/octet_stream";
}

std::optional<std::string> read_item_id(const std::filesystem::path& path) {
#ifdef __APPLE__
    constexpr char attribute[] = "com.google.drivefs.item-id#S";
    const auto size = ::getxattr(path.c_str(), attribute, nullptr, 0, 0, 0);
    if (size <= 0 || size > 512) return std::nullopt;
    std::string value(static_cast<std::size_t>(size), '\0');
    const auto read = ::getxattr(path.c_str(), attribute, value.data(), value.size(), 0, 0);
    if (read <= 0) return std::nullopt;
    value.resize(static_cast<std::size_t>(read));
    while (!value.empty() && value.back() == '\0') value.pop_back();
    return value.empty() ? std::nullopt : std::optional<std::string>(std::move(value));
#else
    (void)path;
    return std::nullopt;
#endif
}

bool is_drive_item_id(std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](const char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_' || character == '-';
    });
}

bool is_ignored_directory(const std::filesystem::path& path) {
    const auto name = path.filename();
    return name == ".Trash" || name == ".tmp" || name == ".shortcut-targets-by-id" ||
           name == ".git" || name == ".obsidian" || name == ".claude";
}

bool is_ignored_file(const std::filesystem::path& path) {
    const auto name = path.filename();
    return name == ".crumb" || name == ".crumb.tmp" || name == ".crumb.index" ||
           name == ".crumb.index.tmp";
}

std::string search_terms(std::string_view content) {
    auto terms = domain::SearchQuery::tokenize(content);
    std::ranges::sort(terms);
    std::string result = "|";
    for (const auto& term : terms) result += term + '|';
    return result;
}
}  // namespace crumb::plugins::google_drive::detail
