#pragma once

#include "lib/ports/search_index_repository.hpp"

namespace crumb::infrastructure {
class BinarySearchIndexRepository final : public ports::SearchIndexRepository {
   public:
    std::expected<void, std::string> save(const domain::DirectoryPath&,
                                          const domain::SearchIndex&) override;
    std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const override;
    std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const override;
};
}  // namespace crumb::infrastructure
