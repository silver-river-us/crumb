#include "application/search_manifest.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <string>
#include <utility>

namespace crumb::application {
namespace {
using Location = std::pair<std::string, std::string>;
using MetadataByLocation = std::map<Location, SearchMatch>;
using Clock = std::chrono::steady_clock;

void add_trace(std::vector<SearchTraceSpan>& trace, std::string name,
               Clock::time_point overall_started, Clock::time_point phase_started,
               std::string detail = {}) {
    const auto offset =
        std::chrono::duration_cast<std::chrono::microseconds>(phase_started - overall_started);
    const auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_started);
    trace.push_back({std::move(name), static_cast<std::uint64_t>(offset.count()),
                     static_cast<std::uint64_t>(duration.count()), std::move(detail)});
}

SearchResult to_result(const domain::SearchIndex& index, const domain::SearchQuery& query,
                       std::size_t limit, const MetadataByLocation& metadata = {}) {
    SearchResult result;
    result.inspected = index.documents.size();
    for (const auto& hit : index.search(query)) {
        const auto& location = index.documents[hit.document_id];
        const auto key = Location{location.directory.value(), location.name.value()};
        if (const auto found = metadata.find(key); found != metadata.end()) {
            auto match = found->second;
            match.score = hit.score;
            result.matches.push_back(std::move(match));
        } else {
            result.matches.push_back({location.directory,
                                      location.name,
                                      hit.score,
                                      {},
                                      location.external_url,
                                      std::nullopt,
                                      std::nullopt,
                                      location.created_ns,
                                      location.modified_ns,
                                      location.file_id.empty()
                                          ? std::nullopt
                                          : std::optional<std::string>(location.file_id)});
        }
    }
    std::ranges::sort(result.matches, [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        if (left.created_ns != right.created_ns) {
            if (!left.created_ns) return false;
            if (!right.created_ns) return true;
            return *left.created_ns > *right.created_ns;
        }
        if (left.modified_ns != right.modified_ns) {
            if (!left.modified_ns) return false;
            if (!right.modified_ns) return true;
            return *left.modified_ns > *right.modified_ns;
        }
        if (left.directory.value() != right.directory.value())
            return left.directory.value() < right.directory.value();
        return left.name.value() < right.name.value();
    });
    if (result.matches.size() > limit) result.matches.resize(limit);
    return result;
}
}  // namespace

std::expected<SearchResult, std::string> SearchManifest::execute(
    const domain::DirectoryPath& directory, std::string_view query, std::size_t limit) const {
    const auto overall_started = Clock::now();
    std::vector<SearchTraceSpan> trace;
    const auto query_started = Clock::now();
    auto search_query = domain::SearchQuery::create(query);
    if (!search_query) return std::unexpected(search_query.error());
    add_trace(trace, "query_parse", overall_started, query_started,
              std::to_string(search_query->words().size()) + " terms");

    // The persisted index is the fast path. A missing index deliberately falls back
    // to the manifest path so the library remains useful before the first scan.
    if (index_) {
        const auto load_started = Clock::now();
        auto loaded_index = index_->load(directory);
        add_trace(trace, "index_load", overall_started, load_started,
                  loaded_index ? "persisted index" : "index miss; using manifests");
        if (loaded_index) {
            const auto search_started = Clock::now();
            auto result = to_result(*loaded_index, *search_query, limit);
            add_trace(trace, "rank_results", overall_started, search_started,
                      std::to_string(result.inspected) + " documents");
            result.trace = std::move(trace);
            return result;
        }
    }

    const auto directories_started = Clock::now();
    auto directories = filesystem_.list_directories_recursive(directory);
    if (!directories) return std::unexpected(directories.error());
    add_trace(trace, "list_directories", overall_started, directories_started,
              std::to_string(directories->size()) + " directories");

    domain::SearchIndexBuilder builder;
    MetadataByLocation metadata;
    const auto manifests_started = Clock::now();
    std::size_t loaded_manifests = 0;
    auto loaded = manifests_.load_many(directories.value());
    if (!loaded) return std::unexpected(loaded.error());
    for (const auto& [current, manifest] : loaded.value()) {
        if (!manifest) continue;
        ++loaded_manifests;
        for (const auto& entry : manifest->files()) {
            builder.add(current, entry);
            metadata.emplace(Location{current.value(), entry.name.value()},
                             SearchMatch{current, entry.name, 0.0, entry.metadata.type,
                                         entry.metadata.external_url, entry.metadata.title,
                                         entry.metadata.author, entry.metadata.created_ns,
                                         entry.metadata.modified_ns, file_id_hash(entry.id)});
        }
    }
    add_trace(trace, "load_manifests", overall_started, manifests_started,
              std::to_string(loaded_manifests) + " manifests");

    const auto build_started = Clock::now();
    auto index = std::move(builder).build();
    add_trace(trace, "build_index", overall_started, build_started,
              std::to_string(index.documents.size()) + " documents");

    const auto search_started = Clock::now();
    auto result = to_result(index, *search_query, limit, metadata);
    add_trace(trace, "rank_results", overall_started, search_started,
              std::to_string(result.inspected) + " documents");
    result.trace = std::move(trace);
    return result;
}
}  // namespace crumb::application
