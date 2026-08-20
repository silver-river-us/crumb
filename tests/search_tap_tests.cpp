#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <optional>
#include <string_view>
#include <vector>

#include "presentation/cli/search_tap.hpp"
#include "files/domain/value_objects/directory_path.hpp"
#include "files/domain/value_objects/file_name.hpp"
#include "search/application/search_manifest/match.hpp"
#include "search/application/search_manifest/result.hpp"
#include "search/application/search_manifest/trace.hpp"

int main() {
    const auto directory = crumb::domain::DirectoryPath::create(".");
    crumb::application::SearchResult result;
    result.inspected = 3;
    result.matches.push_back({directory, crumb::domain::FileName::create("proposal.md"), 1.25,
                              "text/markdown", "https://drive.google.com/open?id=abc_123-xyz",
                              std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                              std::nullopt});
    result.trace = {
        {"query_parse", 0, 14, "2 terms"},
        {"index_load", 14, 32, "persisted index"},
        {"rank_results", 46, 8, "3 documents"},
    };

    const auto text = crumb::boundary::render_search_tap("technical proposal", directory, result,
                                                         crumb::boundary::TapFormat::text);
    assert(text.find("tap query=\"technical proposal\"") != std::string::npos);
    assert(text.find("index_load") != std::string::npos);
    assert(text.find("persisted index") != std::string::npos);
    assert(text.find("32 μs") != std::string::npos);

    const auto html = crumb::boundary::render_search_tap("<proposal>", directory, result,
                                                         crumb::boundary::TapFormat::html);
    assert(html.find("<!doctype html>") != std::string::npos);
    assert(html.find("&lt;proposal&gt;") != std::string::npos);
    assert(html.find("proposal.md") != std::string::npos);
    assert(html.find("https://drive.google.com/open?id=abc_123-xyz") != std::string::npos);
    assert(html.find(">open</a>") != std::string::npos);
    assert(html.find("<th>Created</th>") != std::string::npos);
    assert(html.find("<th>Edited</th>") != std::string::npos);
    assert(html.find("<th>ID</th>") != std::string::npos);
    assert(html.find("width:25.93%") != std::string::npos);

    crumb::application::SearchResult slow_result;
    slow_result.trace = {{"load_manifests", 6'983, 151'340, "65 manifests"}};
    const auto slow_text = crumb::boundary::render_search_tap("application", directory, slow_result,
                                                              crumb::boundary::TapFormat::text);
    assert(slow_text.find("151.3 ms") != std::string::npos);
    crumb::application::SearchResult seconds_result;
    seconds_result.trace = {{"seconds", 0, 2'000'000, ""}};
    const auto seconds_text = crumb::boundary::render_search_tap(
        "application", directory, seconds_result, crumb::boundary::TapFormat::text);
    assert(seconds_text.find("2.00 s") != std::string::npos);

    crumb::application::SearchResult very_slow;
    very_slow.trace = {{"x", 1'000'000'000, 2'000'000'000, ""}};
    const auto escaped_text = crumb::boundary::render_search_tap(
        "slash\\quote\"\n\r", crumb::domain::DirectoryPath::create("dir\"\n"), very_slow,
        crumb::boundary::TapFormat::text);
    assert(escaped_text.find("slash\\\\quote\\\"\\n\\r") != std::string::npos);
    assert(escaped_text.find("2.00 s") != std::string::npos);

    crumb::application::SearchResult empty;
    empty.matches.push_back({crumb::domain::DirectoryPath::create("."),
                             crumb::domain::FileName::create("plain.txt"),
                             0.0,
                             {},
                             std::nullopt,
                             std::nullopt,
                             std::nullopt,
                             std::numeric_limits<std::int64_t>::max(),
                             std::numeric_limits<std::int64_t>::max(),
                             std::nullopt});
    const auto escaped_html =
        crumb::boundary::render_search_tap("<&>\"'", crumb::domain::DirectoryPath::create("<&>\"'"),
                                           empty, crumb::boundary::TapFormat::html);
    assert(escaped_html.find("&amp;") != std::string::npos);
    assert(escaped_html.find("&lt;") != std::string::npos);
    assert(escaped_html.find("&gt;") != std::string::npos);
    assert(escaped_html.find("&quot;") != std::string::npos);
    assert(escaped_html.find("&#39;") != std::string::npos);
}
