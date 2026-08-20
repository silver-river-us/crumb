#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <_stdlib.h>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>

#include "google_drive/infrastructure/google_drive_plugin/details.hpp"
#include "google_drive/infrastructure/google_drive_plugin/testing.hpp"

namespace crumb::plugins::google_drive::detail {
namespace {
std::string shell_quote(const std::string& value) {
    std::string result = "'";
    for (const char character : value)
        result += character == '\'' ? "'\\''" : std::string(1, character);
    return result + '\'';
}

#ifdef __APPLE__
void close_pipe(int descriptors[2]) {
    ::close(descriptors[0]);
    ::close(descriptors[1]);
}

[[noreturn]] void execute_child(const std::string& command, int descriptors[2]) {
    ::dup2(descriptors[1], STDOUT_FILENO);
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    ::execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
    ::_exit(127);
}
#endif
}  // namespace

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
                } else
                    break;
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
    const auto* mode = std::getenv("CRUMB_DRIVE_CONTENT");
    if (mode == nullptr || std::string_view(mode) != "1") return std::nullopt;
    const auto extension = path.extension().string();
    std::string command;
    if (extension == ".pdf")
        command = "pdftotext -layout -- " + shell_quote(path.string()) + " - 2>/dev/null";
    else if (extension == ".doc" || extension == ".docx" || extension == ".rtf" ||
             extension == ".rtfd" || extension == ".html" || extension == ".htm")
        command =
            "/usr/bin/textutil -convert txt -stdout " + shell_quote(path.string()) + " 2>/dev/null";
    else
        return std::nullopt;
    return command_output(command, 64ULL * 1024 * 1024, std::chrono::milliseconds(250));
}

std::optional<std::string> extract_plain_text(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    constexpr std::string_view supported = ".md .txt .json .toml .csv .yaml .yml .xml .log";
    if (!supported.contains(extension)) return std::nullopt;
    return command_output("/bin/cat " + shell_quote(path.string()), 8ULL * 1024 * 1024,
                          std::chrono::milliseconds(150));
}
}  // namespace crumb::plugins::google_drive::detail
