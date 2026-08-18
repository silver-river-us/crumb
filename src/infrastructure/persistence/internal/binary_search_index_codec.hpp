#pragma once

#include "domain/search_index/index.hpp"

#include <expected>
#include <iosfwd>
#include <string>

namespace crumb::infrastructure::detail {
void serialize_index(std::ostream& out, const domain::SearchIndex& index);
std::expected<domain::SearchIndex, std::string> deserialize_index(std::istream& in,
                                                                  bool has_modified_ns,
                                                                  bool has_file_id);
}  // namespace crumb::infrastructure::detail
