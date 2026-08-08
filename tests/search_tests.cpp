#include "application/search_manifest.hpp"
#include "application/rebuild_search_index.hpp"

#include <cassert>
#include <map>
#include <optional>

namespace {
class InMemoryManifestRepository final : public crumb::ports::ManifestRepository {
public:
    std::optional<crumb::domain::DirectoryManifest> manifest;

    std::expected<std::optional<crumb::domain::DirectoryManifest>, std::string>
    load(const crumb::domain::DirectoryPath&) override {
        return manifest;
    }

    std::expected<void, std::string> save(const crumb::domain::DirectoryManifest&) override {
        return {};
    }
};

class InMemoryFileSystem final : public crumb::ports::FileSystem {
public:
    std::map<std::string, std::string> contents;

    std::expected<std::vector<crumb::domain::FileSnapshot>, std::string>
    list_regular_files(const crumb::domain::DirectoryPath&) override {
        return std::vector<crumb::domain::FileSnapshot>{};
    }

    std::expected<void, std::string> move_file(
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName&,
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName&) override {
        return {};
    }

    std::expected<std::vector<crumb::domain::DirectoryPath>, std::string>
    list_directories_recursive(const crumb::domain::DirectoryPath& directory) override {
        return std::vector<crumb::domain::DirectoryPath>{directory};
    }

    std::expected<std::vector<std::pair<crumb::domain::DirectoryPath, crumb::domain::FileName>>, std::string>
    list_regular_files_recursive(const crumb::domain::DirectoryPath& directory) override {
        std::vector<std::pair<crumb::domain::DirectoryPath, crumb::domain::FileName>> result;
        for (const auto& [name, _] : contents) result.emplace_back(directory, crumb::domain::FileName::create(name));
        return result;
    }

    std::expected<std::optional<std::string>, std::string> read_text_file(
        const crumb::domain::DirectoryPath&, const crumb::domain::FileName& name) override {
        const auto found = contents.find(name.value());
        if (found == contents.end()) return std::nullopt;
        return found->second;
    }
};

class InMemorySearchIndexRepository final : public crumb::ports::SearchIndexRepository {
public:
    std::optional<crumb::domain::SearchIndex> index;

    std::expected<void, std::string> save(
        const crumb::domain::DirectoryPath&, const crumb::domain::SearchIndex& value) override {
        index = value;
        return {};
    }

    std::expected<crumb::domain::SearchIndex, std::string> load(
        const crumb::domain::DirectoryPath&) const override {
        if (!index) return std::unexpected("index is not available");
        return *index;
    }

    std::expected<std::uintmax_t, std::string> size(
        const crumb::domain::DirectoryPath&) const override {
        return 0;
    }
};
}

int main() {
    using namespace crumb::domain;

    InMemoryManifestRepository repository;
    InMemoryFileSystem filesystem;
    crumb::application::SearchManifest search(repository, filesystem);
    const auto directory = DirectoryPath::create(".");

    auto missing = search.execute(directory, "anything");
    assert(missing.has_value());
    assert(missing->inspected == 0);
    assert(missing->matches.empty());

    auto manifest = DirectoryManifest::create(
        DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX"), directory);
    FileMetadata proposal_metadata;
    proposal_metadata.type = "text/markdown";
    proposal_metadata.title = "Technical Proposal";
    proposal_metadata.tags = {"architecture", "design"};
    proposal_metadata.extension_fields["crumb.search_terms_v2"] = "this document describes domain driven design";
    manifest.add({
        FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6"),
        FileName::create("proposal.md"),
        proposal_metadata,
        Fingerprint::create("xxh3:73abc14b02e9")});

    FileMetadata report_metadata;
    report_metadata.type = "application/pdf";
    manifest.add({
        FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT"),
        FileName::create("annual.report.pdf"),
        report_metadata,
        Fingerprint::create("xxh3:884fa2c0f7d3")});
    repository.manifest = std::move(manifest);
    filesystem.contents["proposal.md"] = "This document describes domain driven design.";
    filesystem.contents["annual.report.pdf"] = "Annual financial report.";

    InMemorySearchIndexRepository index;
    crumb::application::RebuildSearchIndex rebuild_index(repository, filesystem, index);
    const auto rebuilt = rebuild_index.execute(directory);
    assert(rebuilt.has_value());
    assert(index.index.has_value());
    assert(index.index->documents.size() == 2);
    assert(index.index->terms.size() >= 5);

    auto title_match = search.execute(directory, "technical proposal");
    assert(title_match.has_value());
    assert(title_match->inspected == 2);
    assert(title_match->matches.size() == 1);
    assert(title_match->matches.front().name.value() == "proposal.md");

    auto fuzzy_match = search.execute(directory, "proposl");
    assert(fuzzy_match.has_value());
    assert(fuzzy_match->matches.size() == 1);
    assert(fuzzy_match->matches.front().name.value() == "proposal.md");

    auto content_match = search.execute(directory, "domain driven design");
    assert(content_match.has_value());
    assert(content_match->matches.size() == 1);
    assert(content_match->matches.front().name.value() == "proposal.md");

    auto tag_match = search.execute(directory, "ARCHITECTURE");
    assert(tag_match.has_value());
    assert(tag_match->matches.size() == 1);

    auto extension_match = search.execute(directory, ".PDF");
    assert(extension_match.has_value());
    assert(extension_match->matches.size() == 1);
    assert(extension_match->matches.front().name.value() == "annual.report.pdf");

    auto empty_query = search.execute(directory, "");
    assert(!empty_query.has_value());
}
