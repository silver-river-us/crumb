#include "plugins/google_drive/google_drive_plugin.hpp"

#include <array>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <poll.h>
#include <ranges>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/xattr.h>
#endif

namespace crumb::plugins::google_drive {
namespace testing {
PipeFunction pipe_function = ::pipe;
ForkFunction fork_process = ::fork;
}  // namespace testing

namespace {

#if defined(__clang__)
#define CRUMB_NO_COVERAGE __attribute__((no_profile_instrument_function))
#else
#define CRUMB_NO_COVERAGE
#endif

#ifdef __APPLE__
CRUMB_NO_COVERAGE void close_pipe(int descriptors[2]) {
    ::close(descriptors[0]);
    ::close(descriptors[1]);
}

[[noreturn]] CRUMB_NO_COVERAGE void execute_child(const std::string& command, int descriptors[2]) {
    ::dup2(descriptors[1], STDOUT_FILENO);
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    ::execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
    ::_exit(127);
}
#endif

std::string shell_quote(const std::string& value) {
    std::string result = "'";
    for (const char character : value) {
        if (character == '\'')
            result += "'\\''";
        else
            result += character;
    }
    result += '\'';
    return result;
}

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
    if (value.empty()) return std::nullopt;
    return value;
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
    std::vector<std::string> terms;
    std::string current;
    const auto add = [&terms](std::string word) {
        if (word.size() < 3 || std::ranges::find(terms, word) != terms.end()) return;
        terms.push_back(std::move(word));
    };
    for (const unsigned char character : content) {
        if (std::isalnum(character) || character == '_') {
            current.push_back(static_cast<char>(std::tolower(character)));
        } else if (!current.empty()) {
            if (std::ranges::find(stopwords, current) == stopwords.end()) add(std::move(current));
            current.clear();
        }
    }
    if (!current.empty() && std::ranges::find(stopwords, current) == stopwords.end())
        add(std::move(current));
    std::ranges::sort(terms);
    std::string result = "|";
    for (const auto& term : terms) result += term + "|";
    return result;
}

std::optional<std::string> command_output(const std::string& command, std::size_t limit,
                                          std::chrono::milliseconds timeout) {
#ifdef __APPLE__
    int descriptors[2]{};
    if (testing::pipe_function(descriptors) != 0) return std::nullopt;
    const auto child = testing::fork_process();
    if (child < 0) {
        close_pipe(descriptors);
        return std::nullopt;
    }
    if (child == 0) execute_child(command, descriptors);
    ::close(descriptors[1]);
    const auto flags = ::fcntl(descriptors[0], F_GETFL, 0);
    ::fcntl(descriptors[0], F_SETFL, flags | O_NONBLOCK);
    std::string output;
    std::array<char, 8192> buffer{};
    bool eof = false;
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!eof) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            ::kill(child, SIGKILL);
            ::waitpid(child, &status, 0);
            ::close(descriptors[0]);
            return std::nullopt;
        }
        struct pollfd descriptor {
            descriptors[0], POLLIN, 0
        };
        const auto polled = ::poll(&descriptor, 1, static_cast<int>(remaining.count()));
        if (polled < 0 && errno != EINTR) break;
        if (polled > 0 && (descriptor.revents & (POLLIN | POLLHUP))) {
            while (true) {
                const auto count = ::read(descriptors[0], buffer.data(), buffer.size());
                if (count > 0) {
                    if (output.size() + static_cast<std::size_t>(count) > limit) {
                        ::kill(child, SIGKILL);
                        ::waitpid(child, &status, 0);
                        ::close(descriptors[0]);
                        return std::nullopt;
                    }
                    output.append(buffer.data(), static_cast<std::size_t>(count));
                } else if (count == 0) {
                    eof = true;
                    break;
                } else {
                    break;
                }
            }
        }
        if (::waitpid(child, &status, WNOHANG) == child && eof) break;
    }
    ::close(descriptors[0]);
    if (::waitpid(child, &status, WNOHANG) == 0) {
        ::kill(child, SIGKILL);
        ::waitpid(child, &status, 0);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || output.empty()) return std::nullopt;
    return output;
#else
    (void)command;
    (void)limit;
    (void)timeout;
    return std::nullopt;
#endif
}

std::optional<std::string> extract_office_text(const std::filesystem::path& path) {
    const auto* content_mode = std::getenv("CRUMB_DRIVE_CONTENT");
    if (content_mode == nullptr || std::string_view(content_mode) != "1") return std::nullopt;
    const auto extension = path.extension().string();
    std::string command;
    if (extension == ".pdf") {
        command = "pdftotext -layout -- " + shell_quote(path.string()) + " - 2>/dev/null";
    } else if (extension == ".doc" || extension == ".docx" || extension == ".rtf" ||
               extension == ".rtfd" || extension == ".html" || extension == ".htm") {
        command =
            "/usr/bin/textutil -convert txt -stdout " + shell_quote(path.string()) + " 2>/dev/null";
    } else {
        return std::nullopt;
    }

    return command_output(command, 64ULL * 1024 * 1024, std::chrono::milliseconds(250));
}

std::optional<std::string> extract_plain_text(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    if (extension != ".md" && extension != ".txt" && extension != ".json" && extension != ".toml" &&
        extension != ".csv" && extension != ".yaml" && extension != ".yml" && extension != ".xml" &&
        extension != ".log") {
        return std::nullopt;
    }
    return command_output("/bin/cat " + shell_quote(path.string()), 8ULL * 1024 * 1024,
                          std::chrono::milliseconds(150));
}

std::expected<domain::DirectoryPath, std::string> discover_mount() {
    const auto* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return std::unexpected("HOME is not set");
    const auto cloud_storage = std::filesystem::path(home) / "Library" / "CloudStorage";
    std::vector<std::filesystem::path> mounts;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(cloud_storage)) {
            if (entry.is_directory() &&
                entry.path().filename().string().starts_with("GoogleDrive-")) {
                mounts.push_back(entry.path());
            }
        }
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    std::ranges::sort(mounts);
    if (mounts.empty())
        return std::unexpected("no Google Drive mount found under " + cloud_storage.string());
    if (mounts.size() > 1) {
        return std::unexpected("multiple Google Drive mounts found; pass a mount path explicitly");
    }
    return domain::DirectoryPath::create(mounts.front().string());
}

std::filesystem::path cache_base() {
    if (const auto* xdg_cache = std::getenv("XDG_CACHE_HOME");
        xdg_cache != nullptr && *xdg_cache != '\0') {
        return xdg_cache;
    }
    if (const auto* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".cache";
    }
    return std::filesystem::temp_directory_path();
}

std::filesystem::path cache_root_for(const domain::DirectoryPath& source_root) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char character : source_root.value()) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    std::ostringstream key;
    key << std::hex << hash;
    return cache_base() / "crumb" / "google_drive" / key.str();
}

}  // namespace

void DriveManifestRepository::set_source_root(const domain::DirectoryPath& source_root) {
    source_root_ = source_root.value();
    cache_root_ = cache_root_for(source_root);
}

std::filesystem::path DriveManifestRepository::cache_path(
    const domain::DirectoryPath& source_directory) const {
    const auto relative =
        std::filesystem::path(source_directory.value()).lexically_relative(source_root_);
    return relative.empty() || relative == "." ? cache_root_ : cache_root_ / relative;
}

domain::DirectoryManifest DriveManifestRepository::with_path(
    const domain::DirectoryManifest& manifest, domain::DirectoryPath path) {
    auto copy = domain::DirectoryManifest::create(manifest.id(), std::move(path),
                                                  manifest.generated_at(), manifest.generator());
    for (const auto& entry : manifest.files()) copy.add(entry);
    return copy;
}

std::expected<std::optional<domain::DirectoryManifest>, std::string> DriveManifestRepository::load(
    const domain::DirectoryPath& source_directory) {
    if (source_root_.empty()) return std::unexpected("Drive storage root is not configured");
    auto loaded =
        delegate_.load(domain::DirectoryPath::create(cache_path(source_directory).string()));
    if (!loaded) return std::unexpected(loaded.error());
    if (!loaded.value()) return std::nullopt;
    return std::optional<domain::DirectoryManifest>(with_path(*loaded.value(), source_directory));
}

std::expected<void, std::string> DriveManifestRepository::save(
    const domain::DirectoryManifest& manifest) {
    if (source_root_.empty()) return std::unexpected("Drive storage root is not configured");
    const auto destination = cache_path(manifest.path());
    try {
        std::filesystem::create_directories(destination);
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return delegate_.save(with_path(manifest, domain::DirectoryPath::create(destination.string())));
}

std::expected<void, std::string> DriveSearchIndexRepository::save(
    const domain::DirectoryPath&, const domain::SearchIndex& index) {
    if (cache_root_.empty()) return std::unexpected("Drive storage root is not configured");
    try {
        std::filesystem::create_directories(cache_root_);
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
    return delegate_.save(domain::DirectoryPath::create(cache_root_.string()), index);
}

std::expected<domain::SearchIndex, std::string> DriveSearchIndexRepository::load(
    const domain::DirectoryPath&) const {
    if (cache_root_.empty()) return std::unexpected("Drive storage root is not configured");
    return delegate_.load(domain::DirectoryPath::create(cache_root_.string()));
}

std::expected<std::uintmax_t, std::string> DriveSearchIndexRepository::size(
    const domain::DirectoryPath&) const {
    if (cache_root_.empty()) return std::unexpected("Drive storage root is not configured");
    return delegate_.size(domain::DirectoryPath::create(cache_root_.string()));
}

std::expected<std::vector<domain::FileSnapshot>, std::string> DriveFileSystem::list_regular_files(
    const domain::DirectoryPath& directory) {
    std::vector<domain::FileSnapshot> result;
    try {
        for (const auto& item : std::filesystem::directory_iterator(
                 directory.value(), std::filesystem::directory_options::skip_permission_denied)) {
            if (item.is_symlink() || !item.is_regular_file() || is_ignored_file(item.path()))
                continue;
            const auto name = domain::FileName::create(item.path().filename().string());
            domain::FileMetadata metadata;
            metadata.type = mime_type(name.value());
            metadata.size = item.file_size();
            metadata.modified_ns = file_time_ns(item.last_write_time());
            struct stat info {};
            if (::stat(item.path().c_str(), &info) == 0) {
                metadata.inode = static_cast<std::uintmax_t>(info.st_ino);
                metadata.device = static_cast<std::uintmax_t>(info.st_dev);
#if defined(__APPLE__)
                metadata.created_ns =
                    static_cast<std::int64_t>(info.st_birthtimespec.tv_sec) * 1'000'000'000 +
                    static_cast<std::int64_t>(info.st_birthtimespec.tv_nsec);
#endif
            }
            if (const auto item_id = read_item_id(item.path());
                item_id && is_drive_item_id(*item_id)) {
                metadata.external_url = GoogleDrivePlugin::url_for_item_id(*item_id);
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
            if (iterator->is_directory() && is_ignored_directory(iterator->path())) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (iterator->is_symlink() || is_ignored_file(iterator->path()) ||
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
            if (iterator->is_directory() && is_ignored_directory(iterator->path())) {
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

std::expected<domain::FileMetadata, std::string> DriveMetadataExtractor::extract(
    const domain::DirectoryPath& directory, const domain::FileName& name,
    domain::FileMetadata base) {
    auto metadata = std::move(base);
    const auto dot = name.value().rfind('.');
    metadata.title = name.value().substr(0, dot);
    const auto path = std::filesystem::path(directory.value()) / name.value();
    if (const auto item_id = read_item_id(path); item_id && is_drive_item_id(*item_id)) {
        metadata.external_url = GoogleDrivePlugin::url_for_item_id(*item_id);
    }
    auto content = extract_plain_text(path);
    if (!content) content = extract_office_text(path);
    if (content) {
        metadata.extension_fields["crumb.search_terms_v2"] = search_terms(*content);
    }
    return metadata;
}

std::expected<domain::DirectoryPath, std::string> GoogleDrivePlugin::resolve(
    std::optional<std::string_view> requested_path) const {
    if (requested_path) {
        const auto path = std::filesystem::path(std::string(*requested_path));
        if (!std::filesystem::is_directory(path))
            return std::unexpected("Google Drive path is not a directory: " + path.string());
        return domain::DirectoryPath::create(path.string());
    }
    return discover_mount();
}

std::expected<DriveIndexResult, std::string> GoogleDrivePlugin::index(
    std::optional<std::string_view> requested_path) {
    auto directory = resolve(requested_path);
    if (!directory) return std::unexpected(directory.error());
    manifests_.set_source_root(*directory);
    index_.set_cache_root(cache_root_for(*directory));
    auto reconciled = reconcile_.execute_recursive(*directory);
    if (!reconciled) return std::unexpected(reconciled.error());
    auto rebuilt = rebuild_index_.execute(*directory);
    if (!rebuilt) return std::unexpected(rebuilt.error());
    return DriveIndexResult{*directory, *reconciled};
}

std::expected<application::SearchResult, std::string> GoogleDrivePlugin::search(
    const domain::DirectoryPath& directory, std::string_view query, std::size_t limit) {
    manifests_.set_source_root(directory);
    index_.set_cache_root(cache_root_for(directory));
    return search_.execute(directory, query, limit);
}

std::string GoogleDrivePlugin::url_for_item_id(std::string_view item_id) {
    return "https://drive.google.com/open?id=" + std::string(item_id);
}

}  // namespace crumb::plugins::google_drive
