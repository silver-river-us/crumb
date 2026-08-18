#include "plugins/google_drive/google_drive_plugin/drive_file_system.hpp"

#include "plugins/google_drive/google_drive_plugin/support/details.hpp"
#include "infrastructure/filesystem/native_filesystem.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <sys/stat.h>
#include <utility>

namespace crumb::plugins::google_drive {

std::expected<std::vector<domain::FileSnapshot>, std::string> DriveFileSystem::list_regular_files(
    const domain::DirectoryPath& directory) {
    std::vector<domain::FileSnapshot> result;
    try {
        for (const auto& item : std::filesystem::directory_iterator(
                 directory.value(), std::filesystem::directory_options::skip_permission_denied)) {
            if (item.is_symlink() || !item.is_regular_file() ||
                detail::is_ignored_file(item.path()))
                continue;
            const auto name = domain::FileName::create(item.path().filename().string());
            domain::FileMetadata metadata;
            metadata.type = detail::mime_type(name.value());
            metadata.size = item.file_size();
            metadata.modified_ns = detail::file_time_ns(item.last_write_time());
            struct stat info = {};
            if (::stat(item.path().c_str(), &info) == 0) {
                metadata.inode = static_cast<std::uintmax_t>(info.st_ino);
                metadata.device = static_cast<std::uintmax_t>(info.st_dev);
#if defined(__APPLE__)
                metadata.created_ns =
                    static_cast<std::int64_t>(info.st_birthtimespec.tv_sec) * 1'000'000'000 +
                    static_cast<std::int64_t>(info.st_birthtimespec.tv_nsec);
#endif
            }
            if (const auto item_id = detail::read_item_id(item.path());
                item_id && detail::is_drive_item_id(*item_id)) {
                metadata.external_url = detail::url_for_item_id(*item_id);
            }
            const auto fingerprint = "drive-stat:" + std::to_string(metadata.size) + ":" +
                                     std::to_string(metadata.modified_ns) + ":" +
                                     std::to_string(metadata.inode.value_or(0)) + ":" +
                                     std::to_string(metadata.device.value_or(0));
            result.push_back({name, std::move(metadata), domain::Fingerprint::create(fingerprint)});
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(result, [](const auto& left, const auto& right) {
        return left.name.value() < right.name.value();
    });
    return result;
}

std::expected<std::optional<std::string>, std::string> DriveFileSystem::read_text_file(
    const domain::DirectoryPath& directory, const domain::FileName& name) {
    return native_.read_text_file(directory, name);
}

std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
DriveFileSystem::list_regular_files_recursive(const domain::DirectoryPath& directory) {
    std::vector<std::pair<domain::DirectoryPath, domain::FileName>> result;
    try {
        for (std::filesystem::recursive_directory_iterator iterator(
                 directory.value(), std::filesystem::directory_options::skip_permission_denied),
             end;
             iterator != end; ++iterator) {
            if (iterator->is_directory() && detail::is_ignored_directory(iterator->path())) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (iterator->is_symlink() || detail::is_ignored_file(iterator->path()) ||
                !iterator->is_regular_file())
                continue;
            result.emplace_back(
                domain::DirectoryPath::create(iterator->path().parent_path().string()),
                domain::FileName::create(iterator->path().filename().string()));
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return result;
}

std::expected<std::vector<domain::DirectoryPath>, std::string>
DriveFileSystem::list_directories_recursive(const domain::DirectoryPath& directory) {
    std::vector<domain::DirectoryPath> result{directory};
    try {
        for (std::filesystem::recursive_directory_iterator iterator(
                 directory.value(), std::filesystem::directory_options::skip_permission_denied),
             end;
             iterator != end; ++iterator) {
            if (iterator->is_directory() && detail::is_ignored_directory(iterator->path())) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (iterator->is_symlink() || !iterator->is_directory()) continue;
            result.push_back(domain::DirectoryPath::create(iterator->path().string()));
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(
        result, [](const auto& left, const auto& right) { return left.value() < right.value(); });
    return result;
}

}  // namespace crumb::plugins::google_drive
