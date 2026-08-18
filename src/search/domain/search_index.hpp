#pragma once

// Public domain facade for search. The focused implementation units live in
// search/domain/search_index/ while callers keep this stable include path.
#include "search/domain/search_index/builder.hpp"
#include "search/domain/search_index/index.hpp"
#include "search/domain/search_index/query.hpp"

namespace crumb::domain::search {
using Builder = SearchIndexBuilder;
using Index = SearchIndex;
using Query = SearchQuery;
}  // namespace crumb::domain::search
