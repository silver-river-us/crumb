#include "application/search_manifest.hpp"
#include "application/rebuild_search_index.hpp"

#include <cassert>
#include <map>
#include <optional>

namespace {
class InMemoryManifestRepository final : public crumb::ports::ManifestRepository {
   public:
    std::optional<crumb::domain::DirectoryManifest> manifest;

    std::expected<std::optional<crumb::domain::DirectoryManifest>, std::string> load(
        const crumb::domain::DirectoryPath&) override {
        return manifest;
    }

    std::expected<void, std::string> save(const crumb::domain::DirectoryManifest&) override {
        return {};
    }
};

class InMemoryFileSystem final : public crumb::ports::FileSystem {
   public:
    std::map<std::string, std::string> contents;

    std::expected<std::vector<crumb::domain::FileSnapshot>, std::string> list_regular_files(
        const crumb::domain::DirectoryPath&) override {
        return std::vector<crumb::domain::FileSnapshot>{};
    }

    std::expected<std::vector<crumb::domain::DirectoryPath>, std::string>
    list_directories_recursive(const crumb::domain::DirectoryPath& directory) override {
        return std::vector<crumb::domain::DirectoryPath>{directory};
    }

    std::expected<std::vector<std::pair<crumb::domain::DirectoryPath, crumb::domain::FileName>>,
                  std::string>
    list_regular_files_recursive(const crumb::domain::DirectoryPath& directory) override {
        std::vector<std::pair<crumb::domain::DirectoryPath, crumb::domain::FileName>> result;
        result.reserve(contents.size());
        for (const auto& [name, _] : contents)
            result.emplace_back(directory, crumb::domain::FileName::create(name));
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

    std::expected<void, std::string> save(const crumb::domain::DirectoryPath&,
                                          const crumb::domain::SearchIndex& value) override {
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
}  // namespace

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

    auto manifest =
        DirectoryManifest::create(DirectoryId::create("01K1AB5YZ4QH7M2D8E3F9G6JNX"), directory);
    FileMetadata proposal_metadata;
    proposal_metadata.type = "text/markdown";
    proposal_metadata.title = "Technical Proposal";
    proposal_metadata.external_url = "https://drive.google.com/open?id=abc_123-xyz";
    proposal_metadata.created_ns = 1;
    proposal_metadata.modified_ns = 11;
    proposal_metadata.tags = {"architecture", "design"};
    proposal_metadata.extension_fields["crumb.search_terms_v3"] =
        "this document describes domain driven design";
    proposal_metadata.extension_fields["shared"] = "contract";
    manifest.add({FileId::create("01K1ADN1ZC5R7H4XB8QKMP2TV6"), FileName::create("proposal.md"),
                  proposal_metadata, Fingerprint::create("xxh3:73abc14b02e9")});

    FileMetadata report_metadata;
    report_metadata.type = "application/pdf";
    report_metadata.created_ns = 2;
    report_metadata.modified_ns = 12;
    report_metadata.extension_fields["shared"] = "contract";
    manifest.add({FileId::create("01K1AC4K3N7JZM5F21V6PH8QRT"),
                  FileName::create("annual.report.pdf"), report_metadata,
                  Fingerprint::create("xxh3:884fa2c0f7d3")});
    FileMetadata memo_metadata;
    memo_metadata.type = "text/plain";
    memo_metadata.created_ns = 2;
    memo_metadata.extension_fields["shared"] = "contract";
    manifest.add({FileId::create("01K1AEQ1ZC5R7H4XB8QKMP2TV6"), FileName::create("memo.txt"),
                  memo_metadata, Fingerprint::create("xxh3:99fa2c0f7d3")});
    repository.manifest = std::move(manifest);
    filesystem.contents["proposal.md"] = "This document describes domain driven design.";
    filesystem.contents["annual.report.pdf"] = "Annual financial report.";
    filesystem.contents["memo.txt"] = "A contract memo.";

    InMemorySearchIndexRepository index;
    crumb::application::RebuildSearchIndex rebuild_index(repository, filesystem, index);
    const auto rebuilt = rebuild_index.execute(directory);
    assert(rebuilt.has_value());
    assert(index.index.has_value());
    assert(index.index->documents.size() == 3);
    assert(index.index->terms.size() >= 5);

    crumb::application::SearchManifest indexed_search(repository, filesystem, &index);
    auto indexed_match = indexed_search.execute(directory, "technical proposal");
    assert(indexed_match.has_value());
    assert(indexed_match->trace.size() == 3);
    assert(indexed_match->trace[1].name == "index_load");
    assert(indexed_match->trace[1].detail == "persisted index");
    assert(indexed_match->matches.front().external_url.value() ==
           "https://drive.google.com/open?id=abc_123-xyz");
    assert(indexed_match->matches.front().file_id ==
           file_id_hash(repository.manifest->find(FileName::create("proposal.md"))->id));

    auto title_match = search.execute(directory, "technical proposal");
    assert(title_match.has_value());
    assert(title_match->inspected == 3);
    assert(title_match->matches.size() == 1);
    assert(title_match->matches.front().name.value() == "proposal.md");
    assert(title_match->matches.front().external_url.value() ==
           "https://drive.google.com/open?id=abc_123-xyz");
    assert(title_match->matches.front().created_ns == 1);
    assert(title_match->matches.front().modified_ns == 11);
    assert(title_match->matches.front().file_id ==
           file_id_hash(repository.manifest->find(FileName::create("proposal.md"))->id));
    assert(title_match->trace.size() == 5);
    assert(title_match->trace.front().name == "query_parse");
    assert(title_match->trace.back().name == "rank_results");

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

    auto date_ordered = search.execute(directory, "contract");
    assert(date_ordered.has_value());
    assert(date_ordered->matches.size() == 3);
    assert(date_ordered->matches.front().name.value() == "annual.report.pdf");
    assert(date_ordered->matches.front().created_ns == 2);
    assert(date_ordered->matches.front().modified_ns == 12);

    auto extension_match = search.execute(directory, ".PDF");
    assert(extension_match.has_value());
    assert(extension_match->matches.size() == 1);
    assert(extension_match->matches.front().name.value() == "annual.report.pdf");

    auto empty_query = search.execute(directory, "");
    assert(!empty_query.has_value());

    auto natural_language_query =
        SearchQuery::create("how to find a technical proposal and annual report");
    assert(natural_language_query.has_value());
    assert(natural_language_query->words().size() == 9);
    assert(natural_language_query->words()[0] == "how");
    assert(natural_language_query->words()[1] == "to");
    assert(natural_language_query->words()[2] == "find");
    assert(natural_language_query->words()[3] == "a");
    assert(natural_language_query->words()[4] == "technical");
    assert(natural_language_query->words()[5] == "proposal");
    assert(natural_language_query->words()[6] == "and");
    assert(natural_language_query->words()[7] == "annual");
    assert(natural_language_query->words()[8] == "report");
    const auto multilingual_query = SearchQuery::create("cómo empezar");
    assert(multilingual_query.has_value());
    assert(multilingual_query->words().size() == 2);
    assert(multilingual_query->words()[0] == "cómo");
    assert(multilingual_query->words()[1] == "empezar");

    SearchIndexBuilder relaxed_builder;
    relaxed_builder.add(directory, repository.manifest->files()[0]);
    relaxed_builder.add(directory, repository.manifest->files()[1]);
    const auto relaxed_index = std::move(relaxed_builder).build();
    assert(relaxed_index.search(*natural_language_query).empty());
    assert(!relaxed_index.search_relaxed(*natural_language_query).empty());
    auto relaxed_match =
        search.execute(directory, "how to find a technical proposal and annual report");
    assert(relaxed_match.has_value());
    assert(!relaxed_match->matches.empty());

    SearchIndexBuilder path_builder;
    path_builder.add(
        DirectoryPath::create(
            "Shared drives/Internal Docs/Finances/Active clients/Digital Iron/Contracts & NDA"),
        repository.manifest->files().front());
    path_builder.add(DirectoryPath::create("Shared drives/Internal Docs/Finances/Closed "
                                           "accounts/GSE/Documents & Contracts/Contracts"),
                     repository.manifest->files().front());
    const auto path_index = std::move(path_builder).build();
    auto path_query = SearchQuery::create("internal documents contracts digital iron");
    assert(path_query.has_value());
    assert(path_index.search(*path_query).size() == 1);

    SearchIndexBuilder ranking_builder;
    ranking_builder.add(
        DirectoryPath::create(
            "Shared drives/Internal Docs/Finances/Active clients/Digital/Contracts"),
        repository.manifest->files().front());
    ranking_builder.add(
        DirectoryPath::create(
            "Shared drives/Internal Docs/Finances/Active clients/Digital Iron/Contracts"),
        repository.manifest->files().front());
    const auto ranking_index = std::move(ranking_builder).build();
    const auto ranked_matches = ranking_index.search_relaxed(*path_query);
    assert(ranked_matches.size() == 2);
    assert(ranked_matches.front().document_id == 1);
    assert(ranked_matches.front().score > ranked_matches.back().score);

    SearchIndexBuilder conjunction_builder;
    conjunction_builder.add(directory, repository.manifest->files()[0]);
    conjunction_builder.add(directory, repository.manifest->files()[1]);
    auto conjunction_index = std::move(conjunction_builder).build();
    auto conjunction_query = SearchQuery::create("proposal annual");
    assert(conjunction_query.has_value());
    assert(conjunction_index.search(*conjunction_query).empty());

    SearchIndexBuilder short_term_builder;
    short_term_builder.add(DirectoryPath::create("A"), repository.manifest->files()[0]);
    auto short_term_index = std::move(short_term_builder).build();
    auto short_term_query = SearchQuery::create("a");
    assert(short_term_query.has_value());
    assert(short_term_index.search(*short_term_query).size() == 1);
}
