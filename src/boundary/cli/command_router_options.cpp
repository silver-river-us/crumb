#include "boundary/cli/internal/command_router_internal.hpp"

#include <charconv>
#include <string_view>

namespace crumb::boundary::detail {
namespace {
constexpr std::string_view usage =
    "usage: crumb search [DIRECTORY] QUERY [--limit N] [--full|--table] [--tap [html]]\n";

bool parse_display_option(std::string_view argument, SearchOptions& options) {
    if (argument == "--full" || argument == "--details") {
        options.full_results = true;
        return true;
    }
    if (argument == "--table") {
        options.full_results = false;
        return true;
    }
    return false;
}

std::expected<bool, std::string> parse_tap_option(std::string_view argument, int& index, int argc,
                                                  char** argv, SearchOptions& options) {
    if (argument == "--tap") {
        options.tap_requested = true;
        if (index + 1 < argc && std::string_view(argv[index + 1]) == "html") {
            options.tap_format = TapFormat::html;
            ++index;
        }
        return true;
    }
    if (!argument.starts_with("--tap=")) return false;
    const auto format = argument.substr(6);
    if (format == "html")
        options.tap_format = TapFormat::html;
    else if (format == "text")
        options.tap_format = TapFormat::text;
    else
        return std::unexpected(std::string(usage));
    options.tap_requested = true;
    return true;
}

std::expected<bool, std::string> parse_limit_option(std::string_view argument, int& index, int argc,
                                                    char** argv, SearchOptions& options) {
    std::string_view value;
    if (argument == "--limit") {
        if (index + 1 >= argc) return std::unexpected(std::string(usage));
        value = argv[++index];
    } else if (argument.starts_with("--limit=")) {
        value = argument.substr(8);
    } else {
        return false;
    }
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), options.limit);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        return std::unexpected("crumb: limit must be a nonnegative integer\n");
    return true;
}
}  // namespace

std::expected<SearchOptions, std::string> parse_search_options(int argc, char** argv) {
    if (argc < 3) return std::unexpected(std::string(usage));
    SearchOptions options;
    for (int index = 2; index < argc; ++index) {
        const auto argument = std::string_view(argv[index]);
        if (parse_display_option(argument, options)) continue;
        auto tap = parse_tap_option(argument, index, argc, argv, options);
        if (!tap) return std::unexpected(tap.error());
        if (*tap) continue;
        auto limit = parse_limit_option(argument, index, argc, argv, options);
        if (!limit) return std::unexpected(limit.error());
        if (*limit) continue;
        options.positionals.push_back(argument);
    }
    if (options.positionals.empty() || options.positionals.size() > 2)
        return std::unexpected(std::string(usage));
    return options;
}
}  // namespace crumb::boundary::detail
