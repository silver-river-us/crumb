#pragma once

#include "application/search_manifest/match.hpp"
#include "application/search_manifest/result.hpp"
#include "application/search_manifest/search.hpp"
#include "application/search_manifest/trace.hpp"

namespace crumb::application::search {
using Match = SearchMatch;
using Result = SearchResult;
using Search = SearchManifest;
using Trace = SearchTraceSpan;
}  // namespace crumb::application::search
