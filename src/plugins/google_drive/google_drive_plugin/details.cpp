#include "plugins/google_drive/google_drive_plugin/details.hpp"

#include "domain/search_index/query.hpp"
#include "plugins/google_drive/google_drive_plugin/testing.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include <signal.h>

#ifdef __APPLE__
#include <sys/xattr.h>
#endif

namespace crumb::plugins::google_drive::detail {
namespace {

#if defined(__clang__)
#define CRUMB_NO_COVERAGE __attribute__((no_profile_instrument_function))
#else
#define CRUMB_NO_COVERAGE
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

}  // namespace

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
    auto terms = domain::SearchQuery::tokenize(content);
    std::ranges::sort(terms);
    std::string result = "|";
    for (const auto& term : terms) {
        result += term;
        result += '|';
    }
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
        struct pollfd descriptor = {descriptors[0], POLLIN, 0};
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

std::filesystem::path cache_root_for(const domain::DirectoryPath& source_root) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const char character : source_root.value()) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ull;
    }
    std::ostringstream key;
    key << std::hex << hash;
    return cache_base() / "crumb" / "google_drive" / key.str();
}

std::string url_for_item_id(std::string_view item_id) {
    return "https://drive.google.com/open?id=" + std::string(item_id);
}

}  // namespace crumb::plugins::google_drive::detail
