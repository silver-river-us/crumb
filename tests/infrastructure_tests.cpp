#include "files/infrastructure/native_filesystem.hpp"
#include "search/domain/search_index/builder.hpp"

#include "files/infrastructure/streaming_hash.hpp"
#include "search/infrastructure/binary_search_index_repository.hpp"
#include "manifests/infrastructure/toml_manifest_mapper.hpp"
#include "manifests/infrastructure/toml_manifest_repository.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <string>
#include <sys/stat.h>
#include <tuple>
#include <vector>
#include <zlib.h>

namespace {
using namespace crumb;

const auto id = domain::DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX");
const auto file_id = domain::FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6");

domain::DirectoryManifest manifest_at(const std::filesystem::path& path) {
    auto manifest = domain::DirectoryManifest::create(
        id, domain::DirectoryPath::create(path.string()), "2026-08-11T00:00:00Z", "test");
    domain::FileMetadata metadata;
    metadata.type = "text/markdown";
    metadata.size = 7;
    metadata.modified_ns = 8;
    metadata.created_ns = 6;
    metadata.inode = 4;
    metadata.device = 5;
    metadata.title = "A title";
    metadata.author = "An author";
    metadata.content_hash = "blake3:content";
    metadata.external_url = "https://example.test/item";
    metadata.extractor = "test-extractor";
    metadata.extension_fields["custom.value"] = "\"kept\"";
    manifest.add({file_id, domain::FileName::create("notes.md"), metadata,
                  domain::Fingerprint::create("xxh3:fingerprint")});
    return manifest;
}

std::filesystem::path temp_root(std::string_view name) {
    auto path = std::filesystem::temp_directory_path() /
                (std::string("crumb-") + std::string(name) + "-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

template <typename T>
void append_number(std::string& output, T value) {
    output.append(reinterpret_cast<const char*>(&value), sizeof value);
}

void append_string(std::string& output, std::string_view value) {
    append_number(output, static_cast<std::uint32_t>(value.size()));
    output.append(value);
}

std::string valid_payload(bool modified, bool file_ids) {
    std::string payload;
    append_number(payload, std::uint32_t{1});
    append_string(payload, ".");
    append_string(payload, "notes.md");
    if (file_ids) append_string(payload, "fid:abc");
    append_number(payload, std::uint8_t{1});
    append_string(payload, "https://example.test");
    append_number(payload, std::uint8_t{1});
    append_number(payload, std::int64_t{10});
    if (modified) {
        append_number(payload, std::uint8_t{1});
        append_number(payload, std::int64_t{11});
    }
    append_number(payload, std::uint32_t{1});
    append_string(payload, "notes");
    append_number(payload, std::uint32_t{1});
    append_number(payload, std::uint32_t{0});
    append_number(payload, std::uint32_t{2});
    return payload;
}

void write_index_file(const std::filesystem::path& path, std::string_view magic,
                      const std::string& payload) {
    uLongf compressed_size = compressBound(static_cast<uLong>(payload.size()));
    std::vector<Bytef> compressed(compressed_size);
    assert(compress2(compressed.data(), &compressed_size,
                     reinterpret_cast<const Bytef*>(payload.data()),
                     static_cast<uLong>(payload.size()), Z_BEST_SPEED) == Z_OK);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    const auto raw_size = static_cast<std::uint64_t>(payload.size());
    const auto packed_size = static_cast<std::uint64_t>(compressed_size);
    output.write(reinterpret_cast<const char*>(&raw_size), sizeof raw_size);
    output.write(reinterpret_cast<const char*>(&packed_size), sizeof packed_size);
    output.write(reinterpret_cast<const char*>(compressed.data()),
                 static_cast<std::streamsize>(compressed_size));
}

void binary_repository_tests(const std::filesystem::path& root) {
    infrastructure::BinarySearchIndexRepository repository;
    auto manifest = manifest_at(root);
    domain::SearchIndexBuilder builder;
    builder.add(domain::DirectoryPath::create(root.string()), manifest.files().front());
    auto original = std::move(builder).build();
    const auto saved = repository.save(domain::DirectoryPath::create(root.string()), original);
    assert(saved);
    assert(repository.size(domain::DirectoryPath::create(root.string())).value() > 4);
    auto loaded = repository.load(domain::DirectoryPath::create(root.string()));
    assert(loaded.has_value());
    assert(loaded->documents.size() == 1);
    assert(loaded->documents.front().external_url.has_value());
    assert(loaded->documents.front().created_ns == 6);
    assert(loaded->documents.front().modified_ns == 8);
    assert(!repository.size(domain::DirectoryPath::create((root / "missing").string())));
    assert(!repository.load(domain::DirectoryPath::create((root / "missing").string())));
    std::filesystem::remove(root / ".crumb.index");
    std::filesystem::create_directories(root / ".crumb.index");
    const auto failed_save =
        repository.save(domain::DirectoryPath::create(root.string()), original);
    assert(!failed_save);
    std::filesystem::remove_all(root / ".crumb.index");

    const auto index_path = root / ".crumb.index";
    auto replace = [&](const std::string& bytes) {
        std::ofstream output(index_path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    };
    auto malformed_payload = [&](const std::string& payload, std::string_view magic = "CRZ5") {
        write_index_file(index_path, magic, payload);
        assert(!repository.load(domain::DirectoryPath::create(root.string())));
    };
    {
        std::string payload;
        append_number(payload, std::uint32_t{100'000'001});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, "dir");
        append_string(payload, "name");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint32_t{1});
        append_string(payload, "term");
        append_number(payload, std::uint32_t{1});
        append_number(payload, std::uint32_t{2});
        append_number(payload, std::uint32_t{1});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, ".");
        append_string(payload, "x");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{2});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, ".");
        append_string(payload, "x");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{2});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, ".");
        append_string(payload, "x");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{1});
        append_number(payload, std::uint8_t{1});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, ".");
        append_string(payload, "x");
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{1});
        append_number(payload, std::int64_t{1});
        malformed_payload(payload, "CRZ4");
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, ".");
        append_string(payload, "x");
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{2});
        malformed_payload(payload, "CRZ4");
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, ".");
        append_string(payload, "x");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{1});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{0});
        append_number(payload, std::uint32_t{10'000'001});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{0});
        append_number(payload, std::uint32_t{1});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, "dir");
        append_string(payload, "name");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint32_t{1});
        append_number(payload, std::uint8_t{0});
        append_string(payload, "term");
        append_number(payload, std::uint32_t{1});
        append_number(payload, std::uint32_t{2});
        append_number(payload, std::uint32_t{1});
        malformed_payload(payload);
    }
    {
        std::string payload;
        append_number(payload, std::uint32_t{1});
        append_string(payload, "");
        append_string(payload, "name");
        append_string(payload, "fid");
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint8_t{0});
        append_number(payload, std::uint32_t{0});
        malformed_payload(payload);
    }
    replace("CRZ");
    assert(!repository.load(domain::DirectoryPath::create(root.string())));
    replace("XXXX");
    assert(!repository.load(domain::DirectoryPath::create(root.string())));

    std::string invalid_sizes = "CRZ5";
    append_number(invalid_sizes, std::uint64_t{512ULL * 1024 * 1024 + 1});
    append_number(invalid_sizes, std::uint64_t{0});
    replace(invalid_sizes);
    assert(!repository.load(domain::DirectoryPath::create(root.string())));

    std::string truncated = "CRZ5";
    append_number(truncated, std::uint64_t{4});
    append_number(truncated, std::uint64_t{4});
    truncated += "bad";
    replace(truncated);
    assert(!repository.load(domain::DirectoryPath::create(root.string())));

    std::string bad_compression = "CRZ5";
    append_number(bad_compression, std::uint64_t{4});
    append_number(bad_compression, std::uint64_t{3});
    bad_compression += "bad";
    replace(bad_compression);
    assert(!repository.load(domain::DirectoryPath::create(root.string())));

    for (const auto& [magic, modified, file_ids] : std::vector<std::tuple<std::string, bool, bool>>{
             {"CRZ5", true, true}, {"CRZ4", true, false}, {"CRZ3", false, false}}) {
        write_index_file(index_path, magic, valid_payload(modified, file_ids));
        auto old = repository.load(domain::DirectoryPath::create(root.string()));
        assert(old.has_value());
        assert(old->documents.size() == 1);
        assert(old->documents.front().created_ns == 10);
        if (modified) assert(old->documents.front().modified_ns == 11);
    }
}

void native_and_hash_tests(const std::filesystem::path& root) {
    infrastructure::NativeFileSystem filesystem;
    infrastructure::StreamingHash hashes(root);
    std::ofstream(root / "b.txt") << "Hello, hello!";
    std::ofstream(root / "a.md") << "A document and a document";
    std::ofstream(root / "data.json") << "{\"a\": 1}";
    {
        std::ofstream output(root / "unknown.bin", std::ios::binary);
        const char bytes[] = {'a', '\0', 'b'};
        output.write(bytes, sizeof bytes);
    }
    std::ofstream(root / ".crumb") << "ignored";
    std::ofstream(root / ".crumb.index") << "ignored";
    std::filesystem::create_directories(root / ".git");
    std::ofstream(root / ".git" / "ignored.txt") << "ignored";
    std::filesystem::create_directories(root / "nested");
    std::ofstream(root / "nested" / "note.toml") << "nested";

    auto files = filesystem.list_regular_files(domain::DirectoryPath::create(root.string()));
    assert(files.has_value());
    assert(files->size() == 4);
    assert(files->front().name.value() == "a.md");
    assert(files->front().metadata.type == "text/markdown");
    assert(files->back().name.value() == "unknown.bin");
    auto recursive =
        filesystem.list_regular_files_recursive(domain::DirectoryPath::create(root.string()));
    assert(recursive.has_value());
    assert(recursive->size() == 5);
    auto dirs = filesystem.list_directories_recursive(domain::DirectoryPath::create(root.string()));
    assert(dirs.has_value());
    assert(dirs->size() == 2);
    const auto missing_files =
        filesystem.list_regular_files(domain::DirectoryPath::create((root / "missing").string()));
    assert(!missing_files);
    const auto missing_recursive = filesystem.list_regular_files_recursive(
        domain::DirectoryPath::create((root / "missing").string()));
    assert(!missing_recursive);
    const auto missing_directories = filesystem.list_directories_recursive(
        domain::DirectoryPath::create((root / "missing").string()));
    assert(!missing_directories);
    const auto text = filesystem.read_text_file(domain::DirectoryPath::create(root.string()),
                                                domain::FileName::create("b.txt"));
    assert(text->value() == "Hello, hello!");
    auto binary = filesystem.read_text_file(domain::DirectoryPath::create(root.string()),
                                            domain::FileName::create("unknown.bin"));
    assert(binary.has_value() && !binary->has_value());
    auto missing = filesystem.read_text_file(domain::DirectoryPath::create(root.string()),
                                             domain::FileName::create("missing.txt"));
    assert(missing.has_value() && !missing->has_value());
    const auto invalid_name = domain::FileName::create(std::string(5000, 'x'));
    const auto invalid_text =
        filesystem.read_text_file(domain::DirectoryPath::create(root.string()), invalid_name);
    assert(!invalid_text);
    auto extracted = filesystem.extract(domain::DirectoryPath::create(root.string()),
                                        domain::FileName::create("a.md"), {});
    assert(extracted.has_value());
    assert(extracted->title == "a");
    assert(extracted->extension_fields.at("crumb.search_terms_v3").find("document") !=
           std::string::npos);
    auto binary_extract = filesystem.extract(domain::DirectoryPath::create(root.string()),
                                             domain::FileName::create("unknown.bin"), {});
    assert(binary_extract.has_value());
    assert(binary_extract->extension_fields.empty());

    const auto fingerprint =
        hashes.fingerprint(domain::DirectoryPath::create("."), domain::FileName::create("b.txt"));
    assert(fingerprint.has_value());
    assert(hashes.fingerprint_path(root / "b.txt").has_value());
    const auto content_hash =
        hashes.content_hash(domain::DirectoryPath::create("."), domain::FileName::create("b.txt"));
    assert(content_hash.has_value());
    assert(!hashes.fingerprint_path(root / "missing").has_value());
}

void toml_repository_tests(const std::filesystem::path& root) {
    std::filesystem::create_directories(root / "toml");
    const auto toml_root = root / "toml";
    infrastructure::TomlManifestRepository repository;
    const auto path = domain::DirectoryPath::create(toml_root.string());
    auto missing = repository.load(path);
    assert(missing.has_value() && !missing.value());
    const auto saved = repository.save(manifest_at(toml_root));
    assert(saved);
    auto loaded = repository.load(path);
    assert(loaded.has_value() && loaded->has_value());
    assert(loaded->value().files().front().metadata.author == "An author");
    auto batch =
        repository.load_many({path, domain::DirectoryPath::create((root / "missing").string())});
    assert(batch.has_value() && batch->size() == 2 && !batch->back().second);
    const auto invalid_directory = domain::DirectoryPath::create(std::string("bad\0path", 8));
    auto invalid_batch = repository.load_many({invalid_directory});
    assert(invalid_batch.has_value() && invalid_batch->size() == 1 &&
           !invalid_batch->front().second);

    std::ofstream(toml_root / ".crumb") << "not a manifest";
    const auto invalid_load = repository.load(path);
    assert(!invalid_load);
    std::filesystem::remove(toml_root / ".crumb");
    {
        std::ofstream output(toml_root / ".crumb");
        output << "unreadable";
    }
    assert(::chmod((toml_root / ".crumb").c_str(), 0) == 0);
    const auto unreadable = repository.load(path);
    assert(::chmod((toml_root / ".crumb").c_str(), 0600) == 0);
    assert(!unreadable.has_value());
    std::filesystem::remove(toml_root / ".crumb");
    const auto missing_save = repository.save(manifest_at(toml_root / "missing"));
    assert(!missing_save);
    std::filesystem::create_directories(toml_root / ".crumb");
    const auto directory_save = repository.save(manifest_at(toml_root));
    assert(!directory_save);
    std::filesystem::remove_all(toml_root / ".crumb");

    infrastructure::TomlManifestMapper mapper;
    assert(!mapper.fromToml("version = 1\ndirectory_id = \"bad\"\ngenerated_at = \"now\"\n"));
    assert(!mapper.fromToml("version = 1\ndirectory_id = \"01K1AB5YZ4QH7M2D8E3F9G6JNX\"\n"));
    assert(!mapper.fromToml(
        "version = 1\ndirectory_id = \"01K1AB5YZ4QH7M2D8E3F9G6JNX\"\n"
        "generated_at = \"now\"\n[files.x]\nid = \"01K1ADN1ZC5R7H4XB8QKMP2TV6\"\n"));
    assert(
        !mapper.fromToml("version = 1\ndirectory_id = \"01K1AB5YZ4QH7M2D8E3F9G6JNX\"\n"
                         "generated_at = \"now\"\n[files.x]\n"
                         "id = \"bad\"\ntype = \"text\"\nsize = 1\nmodified_ns = 2\n"
                         "fingerprint = \"xxh3:x\"\n"));
    assert(mapper
               .fromToml("version = 1\ndirectory_id = \"01K1AB5YZ4QH7M2D8E3F9G6JNX\"\n"
                         "generated_at = \"now\"\n generator = \"g\\n\\r\\t\\q\"\n")
               .has_value());
}
}  // namespace

int main() {
    try {
        const auto root = temp_root("infrastructure");
        binary_repository_tests(root);
        native_and_hash_tests(root);
        toml_repository_tests(root);
        std::filesystem::remove_all(root);
    } catch (...) {
        return 1;
    }
    return 0;
}
