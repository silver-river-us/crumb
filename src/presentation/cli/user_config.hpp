#pragma once

#include <expected>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace crumb::boundary {
namespace testing {
std::expected<std::string, std::string> parse_string_for_test(std::string_view value,
                                                              std::size_t line);
bool valid_alias_for_test(std::string_view name);
}  // namespace testing

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
}  // namespace crumb::boundary
