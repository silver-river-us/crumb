#include "boundary/cli/user_config.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("crumb-user-config-" + std::to_string(
                               std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto config_path = directory / ".crumb.conf";
    const auto home = directory / "home";

    {
        std::ofstream output(config_path);
        output << "# Search roots\n[aliases]\nvault = \"~/Develop/vault\"\narchive = \"/Volumes/archive\"\n";
    }

    const auto config = crumb::boundary::UserConfig::load(config_path, home);
    assert(config.has_value());
    assert(config->resolve_directory("vault") == (home / "Develop/vault").string());
    assert(config->resolve_directory("archive") == "/Volumes/archive");
    assert(config->resolve_directory("unaliased") == "unaliased");

    const auto missing = crumb::boundary::UserConfig::load(directory / "missing.conf", home);
    assert(missing.has_value());

    {
        std::ofstream output(config_path);
        output << "[aliases]\ninvalid alias = \"/tmp\"\n";
    }
    const auto invalid = crumb::boundary::UserConfig::load(config_path, home);
    assert(!invalid.has_value());

    std::filesystem::remove_all(directory);
}
