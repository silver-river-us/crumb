#pragma once

#include "search/domain/search_index/index.hpp"
#include <cstdint>
#include <expected>
#include <string>

namespace crumb::ports {
class SearchIndexRepository {
   public:
    virtual ~SearchIndexRepository() = default;
    virtual std::expected<void, std::string> save(const domain::DirectoryPath&,
                                                  const domain::SearchIndex&) = 0;
    virtual std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const = 0;
    virtual std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const = 0;
};
}  // namespace crumb::ports
