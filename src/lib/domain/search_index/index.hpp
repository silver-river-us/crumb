#pragma once

#include "domain/file_entry.hpp"
#include "domain/search_index/query.hpp"
#include "domain/value_objects/directory_path.hpp"
#include "domain/value_objects/file_name.hpp"
#include "domain/value_objects/value_objects.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace crumb::domain {

struct SearchDocument {
    DirectoryPath directory;
    FileName name;
    std::string file_id;
    std::optional<std::string> external_url;
    std::optional<std::int64_t> created_ns;
    std::optional<std::int64_t> modified_ns;
};

struct SearchPosting {
    std::uint32_t document_id{};
    std::uint32_t count{};
};

struct SearchTerm {
    std::string term;
    std::vector<SearchPosting> postings;
};

struct SearchIndex {
    std::vector<SearchDocument> documents;
    std::vector<SearchTerm> terms;

    struct SearchHit {
        std::uint32_t document_id{};
        double score{};
    };

    [[nodiscard]] std::vector<SearchHit> search(const SearchQuery& query) const {
        return search_matching(query, true);
    }

    [[nodiscard]] std::vector<SearchHit> search_relaxed(const SearchQuery& query) const {
        return search_matching(query, false);
    }

   private:
    [[nodiscard]] std::vector<SearchHit> search_matching(const SearchQuery& query,
                                                         bool require_all_terms) const {
        std::map<std::uint32_t, double> scores;
        std::map<std::uint32_t, std::size_t> matched_words;
        const auto total = static_cast<double>(documents.size());

        for (const auto& word : query.words()) {
            std::vector<const SearchTerm*> matching_terms;
            const auto exact = std::ranges::lower_bound(terms, word, {},
                                                        [](const auto& term) { return term.term; });
            if (exact != terms.end() && exact->term == word) {
                matching_terms.push_back(&*exact);
                if (SearchQuery::is_documents_alias(word)) {
                    for (const auto& term : terms) {
                        if (term.term != word && SearchQuery::fuzzy_contains(term.term, word))
                            matching_terms.push_back(&term);
                    }
                }
            } else {
                for (const auto& term : terms) {
                    if (SearchQuery::fuzzy_contains(term.term, word))
                        matching_terms.push_back(&term);
                }
            }

            std::set<std::uint32_t> matched;
            for (const auto* term : matching_terms) {
                for (const auto& posting : term->postings) matched.insert(posting.document_id);
            }
            const auto weight = matched.empty()
                                    ? 0.0
                                    : std::log((total + 1.0) / static_cast<double>(matched.size()));
            for (const auto* term : matching_terms) {
                for (const auto& posting : term->postings)
                    scores[posting.document_id] += weight * posting.count;
            }
            for (const auto document_id : matched) ++matched_words[document_id];
        }

        const auto maximum_relevance =
            std::ranges::max_element(scores, {}, [](const auto& item) { return item.second; });
        const auto relevance_stride =
            maximum_relevance == scores.end() ? 1.0 : maximum_relevance->second + 1.0;
        std::vector<SearchHit> hits;
        for (const auto& [document_id, score] : scores) {
            if (score > 0.0 && document_id < documents.size() &&
                (!require_all_terms || matched_words[document_id] == query.words().size())) {
                const auto coverage_score =
                    static_cast<double>(matched_words[document_id]) * relevance_stride;
                hits.push_back({document_id, coverage_score + score});
            }
        }
        std::ranges::sort(hits, [](const auto& left, const auto& right) {
            if (left.score != right.score) return left.score > right.score;
            return left.document_id < right.document_id;
        });
        return hits;
    }
};

}  // namespace crumb::domain
