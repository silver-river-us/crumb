#include "plugins/google_drive/google_drive_plugin/support/testing.hpp"

#include <cassert>
#include <chrono>

#include <filesystem>
#include <fstream>

namespace {
using namespace crumb;

#ifdef __APPLE__
pid_t fail_fork() { return -1; }
int fail_pipe(int*) { return -1; }
#endif

void command_output_tests() {
#ifdef __APPLE__
    const auto old_pipe = plugins::google_drive::testing::pipe_function;
    const auto old_fork = plugins::google_drive::testing::fork_process;
    plugins::google_drive::testing::pipe_function = fail_pipe;
    assert(!plugins::google_drive::testing::command_output_for_test(
        "printf 'unreachable'", 64, std::chrono::milliseconds(250)));
    plugins::google_drive::testing::pipe_function = old_pipe;
    plugins::google_drive::testing::fork_process = fail_fork;
    assert(!plugins::google_drive::testing::command_output_for_test(
        "printf 'unreachable'", 64, std::chrono::milliseconds(250)));
    plugins::google_drive::testing::fork_process = old_fork;
    const auto output = plugins::google_drive::testing::command_output_for_test(
        "printf 'hello'", 64, std::chrono::milliseconds(250));
    assert(output && *output == "hello");
    assert(!plugins::google_drive::testing::command_output_for_test("sleep 1", 64,
                                                                    std::chrono::milliseconds(1)));
    assert(!plugins::google_drive::testing::command_output_for_test(
        "printf 'too-long'", 3, std::chrono::milliseconds(250)));
#endif
}

void extraction_helper_tests() {
    const auto root = std::filesystem::temp_directory_path() / "crumb-plugin-internal";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto text = root / "note.txt";
    std::ofstream(text) << "hello";
    unsetenv("CRUMB_DRIVE_CONTENT");
    assert(!plugins::google_drive::testing::extract_office_text_for_test(text));
    setenv("CRUMB_DRIVE_CONTENT", "1", 1);
    assert(!plugins::google_drive::testing::extract_office_text_for_test(root / "note.txt"));
    assert(
        !plugins::google_drive::testing::extract_office_text_for_test(root / "note.unsupported"));
    unsetenv("CRUMB_DRIVE_CONTENT");
    std::filesystem::remove_all(root);
}
}  // namespace

int main() {
    command_output_tests();
    extraction_helper_tests();
}
