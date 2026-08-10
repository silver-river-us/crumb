#include "boundary/cli/command_router.hpp"
#include "boundary/cli/search_tap.hpp"
#include "domain/value_objects/value_objects.hpp"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace crumb::boundary {
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
}

int CommandRouter::run(int argc, char** argv) const {
    const std::string command = argc > 1 ? argv[1] : "scan";
    const bool search_command = command == "search";
    const bool index_size_command = command == "index_size";
    if (command != "scan" && !search_command && !index_size_command) {
        std::cerr << "usage: crumb scan [DIRECTORY]\n"
                     "       crumb search [DIRECTORY] QUERY [limit N] [--tap [html]]\n"
                     "       crumb index_size [DIRECTORY]\n";
        return 2;
    }

    if (index_size_command && argc > 3) {
        std::cerr << "usage: crumb index_size [DIRECTORY]\n";
        return 2;
    }

    if (search_command && argc < 3) {
        std::cerr << "usage: crumb search [DIRECTORY] QUERY [limit N] [--tap [html]]\n";
        return 2;
    }

    std::size_t limit = std::numeric_limits<std::size_t>::max();
    bool tap_requested = false;
    TapFormat tap_format = TapFormat::text;
    std::vector<std::string_view> search_positionals;
    if (search_command) {
        for (int index = 2; index < argc; ++index) {
            const auto argument = std::string_view(argv[index]);
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
                std::cerr << "usage: crumb search [DIRECTORY] QUERY [limit N] [--tap [html]]\n";
                return 2;
            }
            if (argument == "limit") {
                if (index + 1 >= argc) {
                    std::cerr << "usage: crumb search [DIRECTORY] QUERY [limit N] [--tap [html]]\n";
                    return 2;
                }
                const auto value = std::string_view(argv[++index]);
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), limit);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                    std::cerr << "crumb: limit must be a nonnegative integer\n";
                    return 2;
                }
                continue;
            }
            search_positionals.push_back(argument);
        }
        if (search_positionals.size() < 1 || search_positionals.size() > 2) {
            std::cerr << "usage: crumb search [DIRECTORY] QUERY [limit N] [--tap [html]]\n";
            return 2;
        }
    }
    const bool explicit_directory = search_command && search_positionals.size() == 2;
    const std::string directory_value = search_command
                                            ? (explicit_directory ? std::string(search_positionals[0]) : ".")
                                            : (argc > 2 ? argv[2] : ".");
    const auto directory = domain::DirectoryPath::create(directory_value);
    const auto query = search_command ? search_positionals.back() : "";

    const auto timer_started = std::chrono::steady_clock::now();

    if (index_size_command) {
        auto result = index_size_.execute(directory);
        if (!result) {
            std::cerr << "crumb: " << result.error() << '\n';
            return 1;
        }
        std::cout << "directory=" << directory.value()
                  << " index_size_bytes=" << *result << '\n';
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
        std::cout << "directory=" << directory.value()
                  << " scanned=" << result->scanned
                  << " added=" << result->added
                  << " updated=" << result->updated
                  << " removed=" << result->removed;
        print_elapsed(std::cout, elapsed);
        std::cout << '\n';
        return 0;
    }

    auto result = search_.execute(directory, query, limit);
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
    std::cout << std::fixed << std::setprecision(4);
    for (const auto& match : result->matches) {
        std::cout << (std::filesystem::path(match.directory.value()) / match.name.value()).string()
                  << " score=" << match.score;
        if (!match.type.empty()) std::cout << " type=" << match.type;
        if (match.title) std::cout << " title=" << *match.title;
        if (match.author) std::cout << " author=" << *match.author;
        std::cout << '\n';
    }
    std::cout << "directory=" << directory.value()
              << " inspected=" << result->inspected
              << " matches=" << result->matches.size();
    print_elapsed(std::cout, elapsed);
    std::cout << '\n';
    if (tap_requested) {
        std::cout << '\n' << render_search_tap(query, directory, *result, TapFormat::text);
    }
    return 0;
}
}
