#include "boundary/cli/user_config.hpp"
#include "infrastructure/persistence/binary_search_index_repository.hpp"
#include "infrastructure/persistence/toml_manifest_repository.hpp"

#include <zlib.h>

#include <cassert>
#include <stdexcept>
#include <sstream>
#include <string>

namespace {
std::expected<std::optional<crumb::domain::DirectoryManifest>, std::string> throwing_load(
    crumb::infrastructure::TomlManifestRepository&, const crumb::domain::DirectoryPath&) {
    throw std::runtime_error("worker failure");
}

template <typename T>
void append_number(std::string& output, T value) {
    output.append(reinterpret_cast<const char*>(&value), sizeof value);
}

void append_string(std::string& output, std::string_view value) {
    append_number(output, static_cast<std::uint32_t>(value.size()));
    output.append(value);
}
}  // namespace

int main() {
    using namespace crumb;

    infrastructure::BinarySearchIndexRepository repository;
    const auto old_compress = infrastructure::testing::compress_function;
    infrastructure::testing::compress_function = [](Bytef*, uLongf*, const Bytef*, uLong, int) {
        return Z_MEM_ERROR;
    };
    const auto save_result =
        repository.save(domain::DirectoryPath::create("."), domain::SearchIndex{});
    infrastructure::testing::compress_function = old_compress;
    assert(!save_result);

    std::string invalid_posting;
    append_number(invalid_posting, std::uint32_t{1});
    append_string(invalid_posting, ".");
    append_string(invalid_posting, "name");
    append_string(invalid_posting, "fid:test");
    append_number(invalid_posting, std::uint8_t{0});
    append_number(invalid_posting, std::uint8_t{0});
    append_number(invalid_posting, std::uint8_t{0});
    append_number(invalid_posting, std::uint32_t{1});
    append_string(invalid_posting, "term");
    append_number(invalid_posting, std::uint32_t{1});
    append_number(invalid_posting, std::uint32_t{2});
    append_number(invalid_posting, std::uint32_t{1});
    std::istringstream invalid_stream(invalid_posting, std::ios::binary);
    assert(!crumb::infrastructure::testing::deserialize_for_test(invalid_stream, true, true));

    std::istringstream malformed_document(std::string(4, '\0'), std::ios::binary);
    assert(!crumb::infrastructure::testing::deserialize_for_test(malformed_document, true, true));

    assert(!crumb::boundary::testing::parse_string_for_test("plain", 1));
    assert(crumb::boundary::testing::valid_alias_for_test("valid-name_1"));
    assert(!crumb::boundary::testing::valid_alias_for_test("invalid alias"));

    const auto old_load_function = crumb::infrastructure::testing::load_function;
    crumb::infrastructure::testing::load_function = throwing_load;
    crumb::infrastructure::TomlManifestRepository toml_repository;
    const auto batch = toml_repository.load_many({crumb::domain::DirectoryPath::create(".")});
    assert(!batch);
    crumb::infrastructure::testing::load_function = old_load_function;
}
