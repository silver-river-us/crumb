#include "plugins/google_drive/google_drive_plugin/testing.hpp"

#include "plugins/google_drive/google_drive_plugin/details.hpp"

#include <unistd.h>

namespace crumb::plugins::google_drive::testing {

PipeFunction pipe_function = ::pipe;
ForkFunction fork_process = ::fork;

std::optional<std::string> command_output_for_test(const std::string& command, std::size_t limit,
                                                   std::chrono::milliseconds timeout) {
    return detail::command_output(command, limit, timeout);
}

std::optional<std::string> extract_office_text_for_test(const std::filesystem::path& path) {
    return detail::extract_office_text(path);
}

}  // namespace crumb::plugins::google_drive::testing
