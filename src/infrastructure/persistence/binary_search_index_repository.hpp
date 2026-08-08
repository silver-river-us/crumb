#pragma once

#include "lib/ports/search_index_repository.hpp"

namespace crumb::infrastructure {
class BinarySearchIndexRepository final : public ports::SearchIndexRepository {
public:
    std::expected<void, std::string> save(const domain::DirectoryPath&, const ports::SearchIndex&) override;
    std::expected<ports::SearchIndex, std::string> load(const domain::DirectoryPath&) const override;
};
}
