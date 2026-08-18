#pragma once

#include "search/domain/search_index/index.hpp"
#include "search/application/search_index_repository.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <utility>

namespace crumb::plugins::google_drive {

class DriveSearchIndexRepository final : public ports::SearchIndexRepository {
   public:
    explicit DriveSearchIndexRepository(ports::SearchIndexRepository& delegate)
        : delegate_(delegate) {}

    void set_cache_root(std::filesystem::path cache_root) { cache_root_ = std::move(cache_root); }
    std::expected<void, std::string> save(const domain::DirectoryPath&,
                                          const domain::SearchIndex&) override;
    std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const override;
    std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const override;

   private:
    ports::SearchIndexRepository& delegate_;
    std::filesystem::path cache_root_;
};

}  // namespace crumb::plugins::google_drive
