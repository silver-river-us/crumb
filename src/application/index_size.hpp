#pragma once

#include "lib/ports/search_index_repository.hpp"

namespace crumb::application {
class IndexSize {
   public:
    explicit IndexSize(ports::SearchIndexRepository& index) : index_(index) {}

    std::expected<std::uintmax_t, std::string> execute(
        const domain::DirectoryPath& directory) const {
        return index_.size(directory);
    }

   private:
    ports::SearchIndexRepository& index_;
};
}  // namespace crumb::application
