#pragma once

#include "lib/ports/search_index_repository.hpp"

#include <istream>
#include <zlib.h>

namespace crumb::infrastructure {
namespace testing {
using CompressFunction = int (*)(Bytef*, uLongf*, const Bytef*, uLong, int);
extern CompressFunction compress_function;
std::expected<domain::SearchIndex, std::string> deserialize_for_test(std::istream& input,
                                                                     bool has_modified_ns,
                                                                     bool has_file_id);
}  // namespace testing

class BinarySearchIndexRepository final : public ports::SearchIndexRepository {
   public:
    std::expected<void, std::string> save(const domain::DirectoryPath&,
                                          const domain::SearchIndex&) override;
    std::expected<domain::SearchIndex, std::string> load(
        const domain::DirectoryPath&) const override;
    std::expected<std::uintmax_t, std::string> size(const domain::DirectoryPath&) const override;
};
}  // namespace crumb::infrastructure
