#include "infrastructure/filesystem/native_filesystem.hpp"
#include "infrastructure/hashing/streaming_hash.hpp"
#include <chrono>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>
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
    return "application/octet_stream";
}
bool is_stopword(std::string_view word) {
    constexpr std::array stopwords{
        "a",    "about", "after",   "all",   "an",     "and",    "any",     "are",   "as",
        "at",   "be",    "because", "been",  "before", "being",  "between", "both",  "but",
        "by",   "can",   "could",   "did",   "do",     "does",   "for",     "from",  "had",
        "has",  "have",  "how",     "if",    "in",     "into",   "is",      "it",    "its",
        "may",  "might", "more",    "most",  "no",     "nor",    "not",     "of",    "on",
        "or",   "our",   "out",     "over",  "same",   "should", "so",      "some",  "such",
        "than", "that",  "the",     "their", "them",   "then",   "there",   "these", "they",
        "this", "those", "to",      "too",   "under",  "until",  "was",     "were",  "what",
        "when", "where", "which",   "while", "who",    "why",    "with",    "would", "you",
        "your"};
    return std::ranges::find(stopwords, word) != stopwords.end();
}

std::string search_terms(std::string_view content) {
    std::vector<std::string> terms;
    std::string current;
    const auto add = [&terms](std::string word) {
        if (word.size() >= 3 && !is_stopword(word) &&
            std::ranges::find(terms, word) == terms.end()) {
            terms.push_back(std::move(word));
        }
    };
    for (const char character : content) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || character == '_') {
            current.push_back(static_cast<char>(std::tolower(byte)));
        } else if (!current.empty()) {
            add(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) add(std::move(current));
    std::ranges::sort(terms);

    std::string result = "|";
    for (const auto& term : terms) result += term + "|";
    return result;
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
            auto status = item.status();
            domain::FileMetadata metadata;
            metadata.type = mime(name.value());
            metadata.size = item.file_size();
            metadata.modified_ns = ns(item.last_write_time());
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
            // This lightweight native adapter provides an inexpensive streamed fingerprint.
            StreamingHash hasher(".");
            auto fingerprint = hasher.fingerprint_path(item.path());
            if (!fingerprint) return std::unexpected(fingerprint.error());
            result.push_back({name, std::move(metadata), std::move(fingerprint.value())});
            (void)status;
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(result,
                      [](const auto& a, const auto& b) { return a.name.value() < b.name.value(); });
    return result;
}

std::expected<domain::FileMetadata, std::string> NativeFileSystem::extract(
    const domain::DirectoryPath& directory, const domain::FileName& name,
    domain::FileMetadata base) {
    const auto dot = name.value().rfind('.');
    base.title = name.value().substr(0, dot);
    auto content = read_text_file(directory, name);
    if (content && content.value())
        base.extension_fields["crumb.search_terms_v2"] = search_terms(*content.value());
    return base;
}
std::expected<std::vector<std::pair<domain::DirectoryPath, domain::FileName>>, std::string>
NativeFileSystem::list_regular_files_recursive(const domain::DirectoryPath& directory) {
    std::vector<std::pair<domain::DirectoryPath, domain::FileName>> result;
    try {
        const auto root = std::filesystem::path(directory.value());
        for (std::filesystem::recursive_directory_iterator
                 iterator(root, std::filesystem::directory_options::skip_permission_denied),
             end;
             iterator != end; ++iterator) {
            const auto name = iterator->path().filename();
            if (iterator->is_directory() &&
                (name == ".git" || name == ".obsidian" || name == ".claude")) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (name == ".crumb" || name == ".crumb.tmp" || name == ".crumb.index" ||
                name == ".crumb.index.tmp" || iterator->is_symlink())
                continue;
            if (!iterator->is_regular_file()) continue;
            result.emplace_back(
                domain::DirectoryPath::create(iterator->path().parent_path().string()),
                domain::FileName::create(name.string()));
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return result;
}
std::expected<std::vector<domain::DirectoryPath>, std::string>
NativeFileSystem::list_directories_recursive(const domain::DirectoryPath& directory) {
    std::vector<domain::DirectoryPath> result;
    try {
        const auto root = std::filesystem::path(directory.value());
        result.push_back(directory);
        for (std::filesystem::recursive_directory_iterator
                 iterator(root, std::filesystem::directory_options::skip_permission_denied),
             end;
             iterator != end; ++iterator) {
            const auto name = iterator->path().filename();
            if (iterator->is_directory() &&
                (name == ".git" || name == ".obsidian" || name == ".claude")) {
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
std::expected<std::optional<std::string>, std::string> NativeFileSystem::read_text_file(
    const domain::DirectoryPath& directory, const domain::FileName& name) {
    constexpr std::uintmax_t max_text_file_size = 8ULL * 1024 * 1024;
    try {
        const auto path = std::filesystem::path(directory.value()) / name.value();
        if (!std::filesystem::is_regular_file(path) ||
            std::filesystem::file_size(path) > max_text_file_size) {
            return std::nullopt;
        }
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::unexpected("cannot read " + path.string());
        std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
        if (content.find('\0') != std::string::npos) return std::nullopt;
        return content;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
}  // namespace crumb::infrastructure
