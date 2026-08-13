#include "boundary/cli/command_router.hpp"
#include "boundary/cli/search_tap.hpp"

#include <charconv>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>
#include <unistd.h>

namespace crumb::boundary {
namespace testing {
PopenFunction popen_function = ::popen;
}  // namespace testing

namespace {

void print_elapsed(std::ostream& output, std::chrono::microseconds elapsed) {
    const auto microseconds = elapsed.count();
    if (microseconds < 1'000) {
        output << " elapsed_µs=" << microseconds;
    } else if (microseconds < 1'000'000) {
        output << std::fixed << std::setprecision(3)
               << " elapsed_ms=" << static_cast<double>(microseconds) / 1'000.0;
    } else {
        output << std::fixed << std::setprecision(6)
               << " elapsed_s=" << static_cast<double>(microseconds) / 1'000'000.0;
    }
}

std::string shorten(std::string_view value, std::size_t width) {
    if (value.size() <= width) return std::string(value);
    if (width <= 3) return std::string(value.substr(0, width));
    const auto suffix = (width - 3) / 2;
    const auto prefix = width - 3 - suffix;
    return std::string(value.substr(0, prefix)) + "..." +
           std::string(value.substr(value.size() - suffix));
}

std::string relative_result_path(const domain::DirectoryPath& root,
                                 const application::SearchMatch& match) {
    std::error_code error;
    const bool invalid_path = root.value().find('\0') != std::string::npos ||
                              match.directory.value().find('\0') != std::string::npos ||
                              match.name.value().find('\0') != std::string::npos;
    const auto root_path = std::filesystem::absolute(root.value(), error);
    const auto full_path = std::filesystem::absolute(
        std::filesystem::path(match.directory.value()) / match.name.value(), error);
    if (invalid_path) error = std::make_error_code(std::errc::invalid_argument);
    if (error)
        return (std::filesystem::path(match.directory.value()) / match.name.value()).string();
    const auto relative = std::filesystem::relative(full_path, root_path, error);
    if (error || relative.empty() || relative.string().starts_with("../"))
        return full_path.string();
    return relative.string();
}

std::string clickable_url(std::string_view url, bool interactive, std::string_view label = {}) {
    if (!interactive) return std::string(url);
    const auto visible = label.empty() ? url : label;
    return "\033]8;;" + std::string(url) + "\a" + std::string(visible) + "\033]8;;\a";
}

std::string format_date(const std::optional<std::int64_t>& timestamp_ns) {
    if (!timestamp_ns) return "-";
    const auto seconds = static_cast<std::time_t>(*timestamp_ns / 1'000'000'000);
    const auto* local = std::localtime(&seconds);
    if (!local) return "-";
    char formatted[11]{};
    if (std::strftime(formatted, sizeof formatted, "%Y-%m-%d", local) == 0) return "-";
    return formatted;
}

void print_search_table(std::ostream& output, const domain::DirectoryPath& directory,
                        const application::SearchResult& result, bool scrollable) {
    std::ostringstream table;
    const bool interactive = scrollable && ::isatty(STDOUT_FILENO) == 1;
    const std::vector<std::size_t> widths{3, 20, 50, 46, 9, 20, 10, 10, 60};
    const auto rule = [&table, &widths](char fill) {
        table << '+';
        for (const auto width : widths) table << std::string(width + 2, fill) << '+';
        table << '\n';
    };
    const auto row = [&table, &widths, interactive](const std::vector<std::string>& values) {
        table << '|';
        for (std::size_t index = 0; index < widths.size(); ++index) {
            const auto value = index < values.size() ? values[index] : "";
            const auto rendered = index == widths.size() - 1 && value != "-"
                                      ? clickable_url(value, interactive)
                                      : value;
            table << ' ' << rendered
                  << std::string(value.size() < widths[index] ? widths[index] - value.size() : 0,
                                 ' ')
                  << ' ' << '|';
        }
        table << '\n';
    };

    table << "Search results: " << result.matches.size() << " matches in " << directory.value()
          << '\n';
    rule('-');
    row({"#", "ID", "Name", "Folder", "Score", "Type", "Created", "Edited", "Link"});
    rule('-');
    for (std::size_t index = 0; index < result.matches.size(); ++index) {
        const auto& match = result.matches[index];
        const auto relative = relative_result_path(directory, match);
        const auto path = std::filesystem::path(relative);
        const auto type = match.type.empty() ? path.extension().string() : match.type;
        row({std::to_string(index + 1), match.file_id.value_or("-"),
             shorten(path.filename().string(), widths[2]),
             shorten(path.parent_path().string(), widths[3]),
             [&] {
                 std::ostringstream score;
                 score << std::fixed << std::setprecision(4) << match.score;
                 return score.str();
             }(),
             shorten(type, widths[5]), format_date(match.created_ns),
             format_date(match.modified_ns), match.external_url.value_or("-")});
    }
    rule('-');
    const auto rendered = table.str();
    if (!scrollable || !interactive) {
        output << rendered;
        return;
    }
    if (FILE* pager = testing::popen_function("less -SRFX", "w")) {
        (void)::fwrite(rendered.data(), 1, rendered.size(), pager);
        (void)::pclose(pager);
        return;
    }
    output << rendered;
}

void print_search_details(std::ostream& output, const domain::DirectoryPath& directory,
                          const application::SearchResult& result) {
    const bool interactive = ::isatty(STDOUT_FILENO) == 1;
    output << "Search results: " << result.matches.size() << " matches in " << directory.value()
           << '\n';
    for (std::size_t index = 0; index < result.matches.size(); ++index) {
        const auto& match = result.matches[index];
        const auto path = std::filesystem::path(match.directory.value()) / match.name.value();
        const auto type = match.type.empty() ? path.extension().string() : match.type;
        output << '\n'
               << '[' << index + 1 << "] " << match.name.value() << '\n'
               << "  id:     " << match.file_id.value_or("-") << '\n'
               << "  path:   " << path.string() << '\n'
               << "  folder: " << path.parent_path().string() << '\n'
               << "  score:  " << std::fixed << std::setprecision(4) << match.score << '\n'
               << "  type:   " << type << '\n'
               << "  created: " << format_date(match.created_ns) << '\n'
               << "  edited:  " << format_date(match.modified_ns) << '\n';
        if (match.title) output << "  title:  " << *match.title << '\n';
        if (match.author) output << "  author: " << *match.author << '\n';
        if (match.external_url) {
            output << "  url:    " << clickable_url(*match.external_url, interactive) << '\n';
        }
    }
}
}  // namespace

namespace testing {
std::string relative_result_path_for_test(const application::SearchMatch& match,
                                          const domain::DirectoryPath& root) {
    return relative_result_path(root, match);
}

std::string clickable_url_for_test(std::string_view url, bool interactive, std::string_view label) {
    return clickable_url(url, interactive, label);
}

std::string shorten_for_test(std::string_view value, std::size_t width) {
    return shorten(value, width);
}

void print_search_table_for_test(std::ostream& output, const domain::DirectoryPath& directory,
                                 const application::SearchResult& result, bool scrollable) {
    print_search_table(output, directory, result, scrollable);
}
}  // namespace testing

int CommandRouter::run(int argc, char** argv) const {
    const std::string command = argc > 1 ? argv[1] : "scan";
    const bool search_command = command == "search";
    const bool index_size_command = command == "index_size";
    const bool drive_index_command = command == "index";
    if (command != "scan" && !search_command && !index_size_command && !drive_index_command) {
        std::cerr
            << "usage: crumb scan [DIRECTORY]\n"
               "       crumb index drive [PATH]\n"
               "       crumb search [DIRECTORY] QUERY [--limit N] [--full|--table] [--tap [html]]\n"
               "       crumb search drive QUERY [--limit N] [--full|--table] [--tap [html]]\n"
               "       crumb index_size [DIRECTORY]\n";
        return 2;
    }

    if (index_size_command && argc > 3) {
        std::cerr << "usage: crumb index_size [DIRECTORY]\n";
        return 2;
    }

    if (drive_index_command) {
        if (argc < 3 || argc > 4 || std::string_view(argv[2]) != "drive") {
            std::cerr << "usage: crumb index drive [PATH]\n";
            return 2;
        }
        const std::optional<std::string_view> requested_path =
            argc == 4 ? std::optional<std::string_view>(argv[3]) : std::nullopt;
        auto result = drive_.index(requested_path);
        if (!result) {
            std::cerr << "crumb: " << result.error() << '\n';
            return 1;
        }
        std::cout << "source=drive directory=" << result->directory.value()
                  << " scanned=" << result->reconcile.scanned
                  << " added=" << result->reconcile.added
                  << " updated=" << result->reconcile.updated
                  << " removed=" << result->reconcile.removed << '\n';
        return 0;
    }

    if (search_command && argc < 3) {
        std::cerr << "usage: crumb search [DIRECTORY] QUERY [--limit N] [--full|--table] [--tap "
                     "[html]]\n";
        return 2;
    }

    std::size_t limit = std::numeric_limits<std::size_t>::max();
    bool full_results = true;
    bool tap_requested = false;
    TapFormat tap_format = TapFormat::text;
    std::vector<std::string_view> search_positionals;
    if (search_command) {
        for (int index = 2; index < argc; ++index) {
            const auto argument = std::string_view(argv[index]);
            if (argument == "--full" || argument == "--details") {
                full_results = true;
                continue;
            }
            if (argument == "--table") {
                full_results = false;
                continue;
            }
            if (argument == "--tap") {
                tap_requested = true;
                if (index + 1 < argc && std::string_view(argv[index + 1]) == "html") {
                    tap_format = TapFormat::html;
                    ++index;
                }
                continue;
            }
            if (argument.starts_with("--tap=")) {
                const auto format = argument.substr(6);
                if (format == "html") {
                    tap_requested = true;
                    tap_format = TapFormat::html;
                    continue;
                }
                if (format == "text") {
                    tap_requested = true;
                    tap_format = TapFormat::text;
                    continue;
                }
                std::cerr << "usage: crumb search [DIRECTORY] QUERY [--limit N] [--full|--table] "
                             "[--tap [html]]\n";
                return 2;
            }
            std::string_view limit_value;
            if (argument == "--limit") {
                if (index + 1 >= argc) {
                    std::cerr << "usage: crumb search [DIRECTORY] QUERY [--limit N] "
                                 "[--full|--table] [--tap [html]]\n";
                    return 2;
                }
                limit_value = argv[++index];
            } else if (argument.starts_with("--limit=")) {
                limit_value = argument.substr(8);
            }
            if (!limit_value.empty() || argument == "--limit=") {
                const auto value = limit_value;
                const auto parsed =
                    std::from_chars(value.data(), value.data() + value.size(), limit);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                    std::cerr << "crumb: limit must be a nonnegative integer\n";
                    return 2;
                }
                continue;
            }
            search_positionals.push_back(argument);
        }
        if (search_positionals.size() < 1 || search_positionals.size() > 2) {
            std::cerr << "usage: crumb search [DIRECTORY] QUERY [--limit N] [--full|--table] "
                         "[--tap [html]]\n";
            return 2;
        }
    }
    const bool drive_search =
        search_command && search_positionals.size() == 2 && search_positionals[0] == "drive";
    const bool explicit_directory =
        search_command && search_positionals.size() == 2 && !drive_search;
    const std::string directory_value =
        drive_search
            ? "."
            : config_.resolve_directory(search_command
                                            ? (explicit_directory ? search_positionals[0] : ".")
                                            : (argc > 2 ? std::string_view(argv[2]) : "."));
    auto directory = domain::DirectoryPath::create(directory_value);
    const auto query =
        search_command ? (drive_search ? search_positionals[1] : search_positionals.back()) : "";

    if (drive_search) {
        auto resolved = drive_.resolve();
        if (!resolved) {
            std::cerr << "crumb: " << resolved.error() << '\n';
            return 1;
        }
        directory = std::move(*resolved);
    }

    const auto timer_started = std::chrono::steady_clock::now();

    if (index_size_command) {
        auto result = index_size_.execute(directory);
        if (!result) {
            std::cerr << "crumb: " << result.error() << '\n';
            return 1;
        }
        std::cout << "directory=" << directory.value() << " index_size_bytes=" << *result << '\n';
        return 0;
    }

    if (command == "scan") {
        auto result = reconcile_.execute_recursive(directory);
        if (!result) {
            std::cerr << "crumb: " << result.error() << '\n';
            return 1;
        }
        auto rebuilt = rebuild_index_.execute(directory);
        if (!rebuilt) {
            std::cerr << "crumb: " << rebuilt.error() << '\n';
            return 1;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - timer_started);
        std::cout << "directory=" << directory.value() << " scanned=" << result->scanned
                  << " added=" << result->added << " updated=" << result->updated
                  << " removed=" << result->removed;
        print_elapsed(std::cout, elapsed);
        std::cout << '\n';
        return 0;
    }

    auto result = drive_search ? drive_.search(directory, query, limit)
                               : search_.execute(directory, query, limit);
    if (!result) {
        std::cerr << "crumb: " << result.error() << '\n';
        return 1;
    }
    if (tap_requested && tap_format == TapFormat::html) {
        std::cout << render_search_tap(query, directory, *result, TapFormat::html);
        return 0;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - timer_started);
    if (full_results)
        print_search_details(std::cout, directory, *result);
    else
        print_search_table(std::cout, directory, *result, true);
    std::cout << "directory=" << directory.value() << " inspected=" << result->inspected
              << " matches=" << result->matches.size();
    print_elapsed(std::cout, elapsed);
    std::cout << '\n';
    if (tap_requested) {
        std::cout << '\n' << render_search_tap(query, directory, *result, TapFormat::text);
    }
    return 0;
}
}  // namespace crumb::boundary
