#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace crumb::boundary {
class UserConfig {
public:
    static std::expected<UserConfig, std::string> load_default();
    static std::expected<UserConfig, std::string> load(const std::filesystem::path& path,
                                                       const std::filesystem::path& home);

    std::string resolve_directory(std::string_view value) const;

private:
    explicit UserConfig(std::filesystem::path home) : home_(std::move(home)) {}

    std::filesystem::path home_;
    std::unordered_map<std::string, std::string> aliases_;
};
}
