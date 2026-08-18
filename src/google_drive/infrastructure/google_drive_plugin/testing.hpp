#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/types.h>

namespace crumb::plugins::google_drive::testing {
using PipeFunction = int (*)(int*);
using ForkFunction = pid_t (*)();
extern PipeFunction pipe_function;
extern ForkFunction fork_process;
std::optional<std::string> command_output_for_test(const std::string& command, std::size_t limit,
                                                   std::chrono::milliseconds timeout);
std::optional<std::string> extract_office_text_for_test(const std::filesystem::path& path);
}  // namespace crumb::plugins::google_drive::testing
