#include <sys/stat.h>
#include <unistd.h>
#include <_stdlib.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <expected>
#include <string_view>

#include "presentation/cli/user_config.hpp"

int main() {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("crumb-user-config-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto config_path = directory / ".crumb.conf";
    const auto home = directory / "home";

    {
        std::ofstream output(config_path);
        output << "# Search roots\n[aliases]\nvault = \"~/Develop/vault\"\narchive = "
                  "\"/Volumes/archive\"\n";
    }

    const auto config = crumb::boundary::UserConfig::load(config_path, home);
    assert(config.has_value());
    assert(config->resolve_directory("vault") == (home / "Develop/vault").string());
    assert(config->resolve_directory("archive") == "/Volumes/archive");
    assert(config->resolve_directory("unaliased") == "unaliased");
    {
        std::ofstream output(config_path);
        output
            << "[aliases]\nroot = \"~\"\nescaped = \"quote: \\\" slash \\\\ line \\n \\r \\t\"\n";
    }
    const auto escaped = crumb::boundary::UserConfig::load(config_path, home);
    assert(escaped.has_value());
    assert(escaped->resolve_directory("root") == home.string());
    assert(escaped->resolve_directory("escaped").find("quote:") != std::string::npos);

    const auto missing = crumb::boundary::UserConfig::load(directory / "missing.conf", home);
    assert(missing.has_value());

    {
        std::ofstream output(config_path);
        output << "[aliases]\ninvalid alias = \"/tmp\"\n";
    }
    const auto invalid = crumb::boundary::UserConfig::load(config_path, home);
    assert(!invalid.has_value());

    const std::vector<std::string> invalid_configs = {"[aliases]\nmissing_equals\n",
                                                      "[aliases]\nvalid = plain\n"};
    for (const auto& text : invalid_configs) {
        std::ofstream output(config_path);
        output << text;
        output.close();
        assert(!crumb::boundary::UserConfig::load(config_path, home));
    }
    std::string unsupported = "[aliases]\nvalid = \"bad";
    unsupported += '\\';
    unsupported += "q\"\n";
    std::ofstream(config_path) << unsupported;
    assert(!crumb::boundary::UserConfig::load(config_path, home));
    std::string dangling = "[aliases]\nvalid = \"bad";
    dangling += '\\';
    dangling += "\"\n";
    std::ofstream(config_path) << dangling;
    assert(!crumb::boundary::UserConfig::load(config_path, home));
    std::filesystem::create_directories(directory / "not-a-file");
    assert(crumb::boundary::UserConfig::load(directory / "not-a-file", home).has_value());
    const std::filesystem::path invalid_path(std::string("bad\0path", 8));
    assert(crumb::boundary::UserConfig::load(invalid_path, home).has_value());
    const auto restricted = directory / "restricted";
    std::filesystem::create_directories(restricted);
    if (::geteuid() != 0) {
        assert(::chmod(restricted.c_str(), 0) == 0);
        const auto inaccessible = crumb::boundary::UserConfig::load(restricted / "config", home);
        assert(::chmod(restricted.c_str(), 0700) == 0);
        assert(!inaccessible.has_value());
    }

    setenv("HOME", home.c_str(), 1);
    assert(crumb::boundary::UserConfig::load_default().has_value());
    unsetenv("HOME");

    std::filesystem::remove_all(directory);
}
