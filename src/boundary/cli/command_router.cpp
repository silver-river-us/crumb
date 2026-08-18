#include "boundary/cli/command_router.hpp"

#include "boundary/cli/internal/command_router_internal.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace crumb::boundary {
namespace {
bool known_command(std::string_view command) {
    return command == "scan" || command == "search" || command == "index_size" ||
           command == "index";
}

void print_usage() {
    std::cerr
        << "usage: crumb scan [DIRECTORY]\n"
           "       crumb index drive [PATH]\n"
           "       crumb search [DIRECTORY] QUERY [--limit N] [--full|--table] [--tap [html]]\n"
           "       crumb search drive QUERY [--limit N] [--full|--table] [--tap [html]]\n"
           "       crumb index_size [DIRECTORY]\n";
}
}  // namespace

int CommandRouter::run(int argc, char** argv) const {
    const std::string command = argc > 1 ? argv[1] : "scan";
    if (!known_command(command)) {
        print_usage();
        return 2;
    }
    if (command == "index") return run_index(argc, argv);
    if (command == "index_size") return run_index_size(argc, argv);
    if (command == "search") return run_search(argc, argv);
    const auto directory = domain::DirectoryPath::create(
        config_.resolve_directory(argc > 2 ? std::string_view(argv[2]) : "."));
    return run_scan(directory);
}

int CommandRouter::run_index(int argc, char** argv) const {
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
              << " scanned=" << result->reconcile.scanned << " added=" << result->reconcile.added
              << " updated=" << result->reconcile.updated
              << " removed=" << result->reconcile.removed << '\n';
    return 0;
}

int CommandRouter::run_index_size(int argc, char** argv) const {
    if (argc > 3) {
        std::cerr << "usage: crumb index_size [DIRECTORY]\n";
        return 2;
    }
    const auto directory = domain::DirectoryPath::create(
        config_.resolve_directory(argc > 2 ? std::string_view(argv[2]) : "."));
    auto result = index_size_.execute(directory);
    if (!result) {
        std::cerr << "crumb: " << result.error() << '\n';
        return 1;
    }
    std::cout << "directory=" << directory.value() << " index_size_bytes=" << *result << '\n';
    return 0;
}

int CommandRouter::run_scan(const domain::DirectoryPath& directory) const {
    const auto started = std::chrono::steady_clock::now();
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
        std::chrono::steady_clock::now() - started);
    std::cout << "directory=" << directory.value() << " scanned=" << result->scanned
              << " added=" << result->added << " updated=" << result->updated
              << " removed=" << result->removed;
    detail::print_elapsed(std::cout, elapsed);
    std::cout << '\n';
    return 0;
}

int CommandRouter::run_search(int argc, char** argv) const {
    auto options = detail::parse_search_options(argc, argv);
    if (!options) {
        std::cerr << options.error();
        return 2;
    }
    const bool drive_search =
        options->positionals.size() == 2 && options->positionals[0] == "drive";
    const bool explicit_directory = options->positionals.size() == 2 && !drive_search;
    const auto directory_value = config_.resolve_directory(
        explicit_directory ? options->positionals[0] : std::string_view("."));
    auto directory = domain::DirectoryPath::create(directory_value);
    const auto query = drive_search ? options->positionals[1] : options->positionals.back();
    if (drive_search) {
        auto resolved = drive_.resolve();
        if (!resolved) {
            std::cerr << "crumb: " << resolved.error() << '\n';
            return 1;
        }
        directory = std::move(*resolved);
    }
    const auto started = std::chrono::steady_clock::now();
    auto result = drive_search ? drive_.search(directory, query, options->limit)
                               : search_.execute(directory, query, options->limit);
    if (!result) {
        std::cerr << "crumb: " << result.error() << '\n';
        return 1;
    }
    if (options->tap_requested && options->tap_format == TapFormat::html) {
        std::cout << render_search_tap(query, directory, *result, TapFormat::html);
        return 0;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started);
    if (options->full_results)
        detail::print_search_details(std::cout, directory, *result);
    else
        detail::print_search_table(std::cout, directory, *result, true);
    std::cout << "directory=" << directory.value() << " inspected=" << result->inspected
              << " matches=" << result->matches.size();
    detail::print_elapsed(std::cout, elapsed);
    std::cout << '\n';
    if (options->tap_requested)
        std::cout << '\n' << render_search_tap(query, directory, *result, TapFormat::text);
    return 0;
}
}  // namespace crumb::boundary
