#include "domain/directory_manifest.hpp"
#include "infrastructure/persistence/toml_manifest_mapper.hpp"

#include <cassert>
#include <string>

int main() {
    using namespace crumb::domain;
    auto manifest = DirectoryManifest::create(DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX"),
                                              DirectoryPath::create("."), "2026_08_05T18:30:00Z",
                                              "crumb/0.1.0");
    FileMetadata metadata;
    metadata.type = "text/markdown";
    metadata.size = 42;
    metadata.modified_ns = 7;
    metadata.title = "Technical Proposal";
    metadata.external_url = "https://drive.google.com/open?id=abc_123-xyz";
    manifest.add({FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6"), FileName::create("zeta file.md"),
                  metadata, Fingerprint::create("xxh3:73abc14b02e9")});
    manifest.add({FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT"),
                  FileName::create("annual.report.pdf"), metadata,
                  Fingerprint::create("xxh3:884fa2c0f7d3")});
    crumb::infrastructure::TomlManifestMapper mapper;
    const auto text = mapper.toToml(manifest);
    assert(text.ends_with('\n'));
    assert(text.find("[files.\"annual.report.pdf\"]") < text.find("[files.\"zeta file.md\"]"));
    assert(text.find("original file contents") == std::string::npos);
    auto parsed = mapper.fromToml(text, DirectoryPath::create("."));
    assert(parsed.has_value());
    assert(parsed->files().size() == 2);
    assert(parsed->find(FileName::create("annual.report.pdf"))->id.value() ==
           "01K1AC4K3N7JZM5F21V6PH8QRT");
    assert(parsed->find(FileName::create("annual.report.pdf"))->metadata.external_url.value() ==
           "https://drive.google.com/open?id=abc_123-xyz");
    parsed->rename(FileName::create("annual.report.pdf"), FileName::create("renamed [final].pdf"));
    assert(parsed->find(FileName::create("renamed [final].pdf"))->id.value() ==
           "01K1AC4K3N7JZM5F21V6PH8QRT");
    auto unknown = text;
    unknown.insert(unknown.find("\n\n[files."), "future_field = \"ignored\"\n");
    auto with_unknown = mapper.fromToml(unknown, DirectoryPath::create("."));
    assert(with_unknown.has_value());
    auto unsupported = mapper.fromToml(
        "version = 2\ndirectory_id = \"01K1AB5YZ4QH7M2D8E3F9G6JNX\"\ngenerated_at = \"now\"\n");
    assert(!unsupported.has_value());
}
