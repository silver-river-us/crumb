#pragma once

#include "search/application/search_manifest/match.hpp"
#include "search/application/search_manifest/trace.hpp"

#include <cstddef>
#include <vector>

namespace crumb::application {

struct SearchResult {
    std::size_t inspected{};
    std::vector<SearchMatch> matches;
    std::vector<SearchTraceSpan> trace;
};

}  // namespace crumb::application
