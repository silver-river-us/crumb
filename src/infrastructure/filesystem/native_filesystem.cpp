#include "infrastructure/filesystem/native_filesystem.hpp"
#include "infrastructure/hashing/streaming_hash.hpp"
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

namespace crumb::infrastructure {
namespace {
std::int64_t ns(std::filesystem::file_time_type time) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
}
std::string mime(const std::string& name) {
    const auto dot = name.rfind('.');
    const auto ext = dot == std::string::npos ? "" : name.substr(dot + 1);
    if (ext == "pdf") return "application/pdf";
    if (ext == "md") return "text/markdown";
    if (ext == "txt") return "text/plain";
    if (ext == "json") return "application/json";
    if (ext == "toml") return "application/toml";
    return "application/octet-stream";
}
}
std::expected<std::vector<domain::FileSnapshot>, std::string> NativeFileSystem::list_regular_files(const domain::DirectoryPath& directory) {
    std::vector<domain::FileSnapshot> result;
    try {
        for (const auto& item : std::filesystem::directory_iterator(directory.value(), std::filesystem::directory_options::skip_permission_denied)) {
            if (item.path().filename() == ".crumb" || item.path().filename() == ".crumb.tmp" ||
                item.is_symlink() || !item.is_regular_file()) continue;
            const auto name = domain::FileName::create(item.path().filename().string());
            auto status = item.status();
            domain::FileMetadata metadata;
            metadata.type = mime(name.value());
            metadata.size = item.file_size();
            metadata.modified_ns = ns(item.last_write_time());
            struct stat info{};
            if (::stat(item.path().c_str(), &info) == 0) { metadata.inode = static_cast<std::uintmax_t>(info.st_ino); metadata.device = static_cast<std::uintmax_t>(info.st_dev); }
            // This lightweight native adapter provides an inexpensive streamed fingerprint.
            StreamingHash hasher(".");
            auto fingerprint = hasher.fingerprint_path(item.path());
            if (!fingerprint) return std::unexpected(fingerprint.error());
            result.push_back({name, std::move(metadata), std::move(fingerprint.value())});
            (void)status;
        }
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
    std::ranges::sort(result, [](const auto& a, const auto& b) { return a.name.value() < b.name.value(); });
    return result;
}
std::expected<void, std::string> NativeFileSystem::move_file(const domain::DirectoryPath& source, const domain::FileName& old_name,
                                                              const domain::DirectoryPath& destination, const domain::FileName& new_name) {
    try { std::filesystem::rename(std::filesystem::path(source.value()) / old_name.value(), std::filesystem::path(destination.value()) / new_name.value()); return {}; }
    catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<domain::FileMetadata, std::string> NativeFileSystem::extract(const domain::DirectoryPath&, const domain::FileName& name, domain::FileMetadata base) {
    const auto dot = name.value().rfind('.');
    base.title = name.value().substr(0, dot);
    return base;
}
}
