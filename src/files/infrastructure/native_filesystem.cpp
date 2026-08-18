#include "files/infrastructure/native_filesystem.hpp"

#include "files/infrastructure/streaming_hash.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <sys/stat.h>

namespace crumb::infrastructure {
namespace {
std::int64_t file_time_ns(std::filesystem::file_time_type time) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}

std::string mime_type(const std::string& name) {
    const auto dot = name.rfind('.');
    const auto extension = dot == std::string::npos ? "" : name.substr(dot + 1);
    if (extension == "pdf") return "application/pdf";
    if (extension == "md") return "text/markdown";
    if (extension == "txt") return "text/plain";
    if (extension == "json") return "application/json";
    if (extension == "toml") return "application/toml";
    return "application/octet_stream";
}

}  // namespace

std::expected<std::vector<domain::FileSnapshot>, std::string> NativeFileSystem::list_regular_files(
    const domain::DirectoryPath& directory) {
    std::vector<domain::FileSnapshot> result;
    try {
        for (const auto& item : std::filesystem::directory_iterator(
                 directory.value(), std::filesystem::directory_options::skip_permission_denied)) {
            if (item.path().filename() == ".crumb" || item.path().filename() == ".crumb.tmp" ||
                item.path().filename() == ".crumb.index" || item.is_symlink() ||
                !item.is_regular_file())
                continue;
            const auto name = domain::FileName::create(item.path().filename().string());
            domain::FileMetadata metadata;
            metadata.type = mime_type(name.value());
            metadata.size = item.file_size();
            metadata.modified_ns = file_time_ns(item.last_write_time());
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
            StreamingHash hasher(".");
            auto fingerprint = hasher.fingerprint_path(item.path());
            if (!fingerprint) return std::unexpected(fingerprint.error());
            result.push_back({name, std::move(metadata), std::move(fingerprint.value())});
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(result, [](const auto& left, const auto& right) {
        return left.name.value() < right.name.value();
    });
    return result;
}
}  // namespace crumb::infrastructure
