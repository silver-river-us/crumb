#include "boundary/cli/command_router.hpp"
#include "boundary/cli/search_tap.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace crumb;

std::tm* no_local_time(const std::time_t*) { return nullptr; }

domain::DirectoryPath path(std::string value) {
    return domain::DirectoryPath::create(std::move(value));
}

application::SearchResult result_with_url() {
    application::SearchResult result;
    result.matches.push_back({path("/tmp/outside"), domain::FileName::create("a-very-long-name.md"),
                              1.0, "text/markdown", "https://example.test/item", std::nullopt,
                              std::nullopt, 1'700'000'000'000'000'000, 1'700'000'001'000'000'000,
                              "fid:test"});
    return result;
}

void formatting_helpers() {
    assert(crumb::boundary::format_result_date_for_test(std::nullopt) == "-");
    assert(crumb::boundary::format_result_date_for_test(1'700'000'000'000'000'000, no_local_time) ==
           "-");
    assert(crumb::boundary::testing::shorten_for_test("abc", 3) == "abc");
    assert(crumb::boundary::testing::shorten_for_test("abcdefgh", 8) == "abcdefgh");
    assert(crumb::boundary::testing::shorten_for_test("abcdefgh", 7) == "ab...gh");
    assert(crumb::boundary::testing::shorten_for_test("abcdefgh", 2) == "ab");
    assert(crumb::boundary::testing::clickable_url_for_test("https://example.test", true, "open")
               .find("open") != std::string::npos);

    const auto invalid_root = path(std::string("bad\0root", 8));
    const auto match = result_with_url().matches.front();
    const auto fallback =
        crumb::boundary::testing::relative_result_path_for_test(match, invalid_root);
    assert(!fallback.empty());

    const auto old_current = std::filesystem::current_path();
    const auto removed_current = std::filesystem::temp_directory_path() / "crumb-removed-cwd";
    std::filesystem::remove_all(removed_current);
    std::filesystem::create_directories(removed_current);
    std::filesystem::current_path(removed_current);
    std::filesystem::remove_all(removed_current);
    auto relative_match = match;
    relative_match.directory = path("outside");
    const auto error_path =
        crumb::boundary::testing::relative_result_path_for_test(relative_match, path("."));
    assert(!error_path.empty());
    std::filesystem::current_path(old_current);
}

void pager_helper() {
    const auto root = std::filesystem::temp_directory_path() / "crumb-boundary-internal";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto fake_less = root / "less";
    {
        std::ofstream script(fake_less);
        script << "#!/bin/sh\ncat >/dev/null\nexit 0\n";
    }
    (void)::chmod(fake_less.c_str(), 0755);

    const auto old_path = std::getenv("PATH");
    const auto path_value = root.string() + ":" + (old_path == nullptr ? "" : old_path);
    setenv("PATH", path_value.c_str(), 1);

    const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0);
    assert(::grantpt(master) == 0);
    assert(::unlockpt(master) == 0);
    const char* slave_name = ::ptsname(master);
    assert(slave_name != nullptr);
    const int slave = ::open(slave_name, O_RDWR | O_NOCTTY);
    assert(slave >= 0);
    const int saved_stdout = ::dup(STDOUT_FILENO);
    assert(saved_stdout >= 0);
    assert(::dup2(slave, STDOUT_FILENO) >= 0);

    std::ostringstream ignored;
    crumb::boundary::testing::print_search_table_for_test(ignored, path("/tmp/root"),
                                                          result_with_url(), true);

    struct rlimit old_limit = {};
    assert(::getrlimit(RLIMIT_NOFILE, &old_limit) == 0);
    struct rlimit exhausted_limit = {0, old_limit.rlim_max};
    assert(::setrlimit(RLIMIT_NOFILE, &exhausted_limit) == 0);
    std::ostringstream fallback;
    crumb::boundary::testing::print_search_table_for_test(fallback, path("/tmp/root"),
                                                          result_with_url(), true);
    assert(fallback.str().find("Search results:") != std::string::npos);
    assert(::setrlimit(RLIMIT_NOFILE, &old_limit) == 0);

    assert(::dup2(saved_stdout, STDOUT_FILENO) >= 0);
    ::close(saved_stdout);
    ::close(slave);
    ::close(master);
    if (old_path == nullptr)
        unsetenv("PATH");
    else
        setenv("PATH", old_path, 1);
    std::filesystem::remove_all(root);
}
}  // namespace

int main() {
    formatting_helpers();
    pager_helper();
}
