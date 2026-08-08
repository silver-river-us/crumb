#include "boundary/cli/command_router.hpp"
#include "domain/value_objects/value_objects.hpp"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

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
    if (command != "scan" && command != "search") {
        std::cerr << "usage: crumb scan [DIRECTORY]\n"
                     "       crumb search [DIRECTORY] QUERY [--limit N]\n";
        return 2;
    }

    if (command == "search" && argc < 3) {
        std::cerr << "usage: crumb search [DIRECTORY] QUERY [--limit N]\n";
        return 2;
    }
    const bool search_command = command == "search";
    const bool explicit_directory = search_command && argc >= 4 && std::string_view(argv[3]) != "--limit";
    const int query_index = explicit_directory ? 3 : 2;
    std::size_t limit = std::numeric_limits<std::size_t>::max();
    if (search_command) {
        for (int index = query_index + 1; index < argc; ++index) {
            if (std::string_view(argv[index]) != "--limit" || index + 1 >= argc) {
                std::cerr << "usage: crumb search [DIRECTORY] QUERY [--limit N]\n";
                return 2;
            }
            const auto value = std::string_view(argv[++index]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), limit);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                std::cerr << "crumb: --limit must be a non-negative integer\n";
                return 2;
            }
        }
    }
    const auto directory = domain::DirectoryPath::create(
        search_command ? (explicit_directory ? argv[2] : ".") : (argc > 2 ? argv[2] : "."));
    const auto query = search_command ? argv[query_index] : "";

    const auto timer_started = std::chrono::steady_clock::now();

    if (command == "scan") {
        auto result = reconcile_.execute_recursive(directory);
        if (!result) {
            std::cerr << "crumb: " << result.error() << '\n';
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
    return 0;
}
}
