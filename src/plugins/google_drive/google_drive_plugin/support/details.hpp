#pragma once

#include "domain/value_objects/directory_path.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace crumb::plugins::google_drive::detail {
std::int64_t file_time_ns(std::filesystem::file_time_type time);
std::string mime_type(const std::string& name);
std::optional<std::string> read_item_id(const std::filesystem::path& path);
bool is_drive_item_id(std::string_view value);
bool is_ignored_directory(const std::filesystem::path& path);
bool is_ignored_file(const std::filesystem::path& path);
std::string search_terms(std::string_view content);
std::optional<std::string> command_output(const std::string& command, std::size_t limit,
                                          std::chrono::milliseconds timeout);
std::optional<std::string> extract_office_text(const std::filesystem::path& path);
std::optional<std::string> extract_plain_text(const std::filesystem::path& path);
std::expected<domain::DirectoryPath, std::string> discover_mount();
std::filesystem::path cache_root_for(const domain::DirectoryPath& source_root);
std::string url_for_item_id(std::string_view item_id);
}  // namespace crumb::plugins::google_drive::detail
