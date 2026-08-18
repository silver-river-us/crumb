#include "files/infrastructure/native_filesystem.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>

namespace crumb::infrastructure {
namespace {
bool ignored_directory(const std::filesystem::path& path) {
    const auto name = path.filename();
    return name == ".git" || name == ".obsidian" || name == ".claude";
}

bool ignored_entry(const std::filesystem::path& path) {
    const auto name = path.filename();
    return name == ".crumb" || name == ".crumb.tmp" || name == ".crumb.index" ||
           name == ".crumb.index.tmp";
}
}  // namespace

std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
NativeFileSystem::list_regular_files_recursive(const domain::DirectoryPath& directory) {
    std::vector<std::pair<domain::DirectoryPath, domain::FileName>> result;
    try {
        for (std::filesystem::recursive_directory_iterator
                 iterator(std::filesystem::path(directory.value()),
                          std::filesystem::directory_options::skip_permission_denied),
             end;
             iterator != end; ++iterator) {
            const auto path = iterator->path();
            if (iterator->is_directory() && ignored_directory(path)) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (ignored_entry(path) || iterator->is_symlink() || !iterator->is_regular_file())
                continue;
            result.emplace_back(domain::DirectoryPath::create(path.parent_path().string()),
                                domain::FileName::create(path.filename().string()));
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return result;
}

std::expected<std::vector<domain::DirectoryPath>, std::string>
NativeFileSystem::list_directories_recursive(const domain::DirectoryPath& directory) {
    std::vector<domain::DirectoryPath> result{directory};
    try {
        for (std::filesystem::recursive_directory_iterator
                 iterator(std::filesystem::path(directory.value()),
                          std::filesystem::directory_options::skip_permission_denied),
             end;
             iterator != end; ++iterator) {
            const auto path = iterator->path();
            if (iterator->is_directory() && ignored_directory(path)) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (!iterator->is_symlink() && iterator->is_directory())
                result.push_back(domain::DirectoryPath::create(path.string()));
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(
        result, [](const auto& left, const auto& right) { return left.value() < right.value(); });
    return result;
}
}  // namespace crumb::infrastructure
