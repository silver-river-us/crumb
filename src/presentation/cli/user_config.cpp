#include "presentation/cli/user_config.hpp"

#include <cctype>
#include <cstdlib>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace crumb::boundary {
namespace {
std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::expected<std::string, std::string> parse_string(std::string_view value, std::size_t line) {
    value = trim(value);
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
        return std::unexpected("invalid string at line " + std::to_string(line));
    }

    std::string result;
    for (std::size_t index = 1; index + 1 < value.size(); ++index) {
        if (value[index] != '\\') {
            result += value[index];
            continue;
        }
        if (++index + 1 >= value.size()) {
            return std::unexpected("invalid escape sequence at line " + std::to_string(line));
        }
        switch (value[index]) {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            default:
                return std::unexpected("unsupported escape sequence at line " +
                                       std::to_string(line));
        }
    }
    return result;
}

bool valid_alias(std::string_view name) {
    if (name.empty()) return false;
    for (const auto character : name) {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' &&
            character != '-') {
            return false;
        }
    }
    return true;
}

std::filesystem::path default_home() {
    if (const auto* home = std::getenv("HOME"); home && *home) return home;
    return {};
}
}  // namespace

namespace testing {
std::expected<std::string, std::string> parse_string_for_test(std::string_view value,
                                                              std::size_t line) {
    return parse_string(value, line);
}

bool valid_alias_for_test(std::string_view name) { return valid_alias(name); }
}  // namespace testing

std::expected<UserConfig, std::string> UserConfig::load_default() {
    const auto home = default_home();
    if (home.empty()) return std::unexpected("cannot determine the user's home directory");
    return load(home / ".crumb.conf", home);
}

std::expected<UserConfig, std::string> UserConfig::load(const std::filesystem::path& path,
                                                        const std::filesystem::path& home) {
    UserConfig config(home);
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error)
            return std::unexpected("cannot access configuration file " + path.string() + ": " +
                                   error.message());
        return config;
    }

    std::ifstream input(path);
    if (!input) return std::unexpected("cannot read configuration file " + path.string());

    bool aliases_section = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            aliases_section = trimmed == "[aliases]";
            continue;
        }
        if (!aliases_section) continue;

        const auto equals = trimmed.find('=');
        if (equals == std::string_view::npos) {
            return std::unexpected("invalid alias at line " + std::to_string(line_number));
        }
        const auto name = trim(trimmed.substr(0, equals));
        if (!valid_alias(name)) {
            return std::unexpected("invalid alias name at line " + std::to_string(line_number));
        }
        auto directory = parse_string(trimmed.substr(equals + 1), line_number);
        if (!directory) return std::unexpected(directory.error());
        config.aliases_[std::string(name)] = std::move(*directory);
    }
    if (input.bad()) return std::unexpected("cannot read configuration file " + path.string());
    return config;
}

std::string UserConfig::resolve_directory(std::string_view value) const {
    const auto alias = aliases_.find(std::string(value));
    if (alias == aliases_.end()) return std::string(value);

    const auto& directory = alias->second;
    if (directory == "~") return home_.string();
    if (directory.starts_with("~/")) return (home_ / directory.substr(2)).string();
    return directory;
}
}  // namespace crumb::boundary
