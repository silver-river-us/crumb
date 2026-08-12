#include "boundary/cli/search_tap.hpp"

#include <cassert>
#include <string>

int main() {
    const auto directory = crumb::domain::DirectoryPath::create(".");
    crumb::application::SearchResult result;
    result.inspected = 3;
    result.matches.push_back({directory, crumb::domain::FileName::create("proposal.md"), 1.25,
                              "text/markdown", "https://drive.google.com/open?id=abc_123-xyz",
                              std::nullopt, std::nullopt});
    result.trace = {
        {"query_parse", 0, 14, "2 terms"},
        {"index_load", 14, 32, "persisted index"},
        {"rank_results", 46, 8, "3 documents"},
    };

    const auto text = crumb::boundary::render_search_tap(
        "technical proposal", directory, result, crumb::boundary::TapFormat::text);
    assert(text.find("tap query=\"technical proposal\"") != std::string::npos);
    assert(text.find("index_load") != std::string::npos);
    assert(text.find("persisted index") != std::string::npos);
    assert(text.find("32 μs") != std::string::npos);

    const auto html = crumb::boundary::render_search_tap(
        "<proposal>", directory, result, crumb::boundary::TapFormat::html);
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
    const auto slow_text = crumb::boundary::render_search_tap(
        "application", directory, slow_result, crumb::boundary::TapFormat::text);
    assert(slow_text.find("151.3 ms") != std::string::npos);
}
