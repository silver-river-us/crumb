#pragma once

#include "domain/file_entry.hpp"
#include "domain/value_objects/directory_path.hpp"
#include "domain/value_objects/file_name.hpp"
#include "domain/value_objects/value_objects.hpp"

#include <algorithm>

#include <cstdint>
#include <cmath>
#include <expected>
#include <map>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace crumb::domain {

class SearchQuery {
   public:
    [[nodiscard]] static std::vector<std::string> tokenize(std::string_view value) {
        std::vector<std::string> words;
        std::string current;
        const auto add = [&words](std::string word) {
            if (!word.empty() && std::ranges::find(words, word) == words.end())
                words.push_back(std::move(word));
        };
        const auto flush = [&] {
            if (!current.empty()) {
                add(std::move(current));
                current.clear();
            }
        };

        for (const auto character : value) {
            const auto byte = static_cast<unsigned char>(character);
            if (byte >= 0x80 || std::isalnum(byte) || character == '_') {
                current.push_back(byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a')
                                                             : character);
            } else {
                flush();
            }
        }
        flush();
        return words;
    }

    [[nodiscard]] static std::expected<SearchQuery, std::string> create(std::string_view value) {
        auto words = tokenize(value);
        if (words.empty()) return std::unexpected("search query must contain a word");
        return SearchQuery(std::move(words));
    }

    [[nodiscard]] const std::vector<std::string>& words() const noexcept { return words_; }

    [[nodiscard]] static bool fuzzy_contains(const std::string& value, const std::string& word) {
        if (word.size() < 3) return value == word;
        const auto prefix_length = std::max<std::size_t>(4, word.size() - 2);
        return value.find(word) != std::string::npos ||
               value.find(word.substr(0, prefix_length)) != std::string::npos;
    }

   private:
    explicit SearchQuery(std::vector<std::string> words) : words_(std::move(words)) {}

    std::vector<std::string> words_;
};

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

        std::vector<SearchHit> hits;
        for (const auto& [document_id, score] : scores) {
            if (score > 0.0 && document_id < documents.size() &&
                (!require_all_terms || matched_words[document_id] == query.words().size())) {
                hits.push_back({document_id, score});
            }
        }
        return hits;
    }
};

class SearchIndexBuilder {
   public:
    void add(DirectoryPath directory, const FileEntry& entry) {
        const auto document_id = static_cast<std::uint32_t>(index_.documents.size());
        const auto directory_value = directory.value();
        index_.documents.push_back({std::move(directory), entry.name, file_id_hash(entry.id),
                                    entry.metadata.external_url, entry.metadata.created_ns,
                                    entry.metadata.modified_ns});

        std::unordered_map<std::string, std::uint32_t> terms;
        add_terms(terms, directory_value);
        add_terms(terms, entry.name.value());
        add_terms(terms, entry.metadata.type);
        if (entry.metadata.title) add_terms(terms, *entry.metadata.title);
        if (entry.metadata.author) add_terms(terms, *entry.metadata.author);
        for (const auto& tag : entry.metadata.tags) add_terms(terms, tag);
        for (const auto& [key, value] : entry.metadata.extension_fields) {
            std::string text = key;
            text += ' ';
            text += value;
            add_terms(terms, text);
        }
        for (const auto& [term, count] : terms) postings_[term][document_id] = count;
    }

    [[nodiscard]] SearchIndex build() && {
        for (auto& [term, postings] : postings_) {
            SearchTerm item{term, {}};
            for (const auto& [document_id, count] : postings)
                item.postings.push_back({document_id, count});
            std::ranges::sort(item.postings, {}, &SearchPosting::document_id);
            index_.terms.push_back(std::move(item));
        }
        std::ranges::sort(index_.terms, {}, &SearchTerm::term);
        return std::move(index_);
    }

   private:
    static void add_terms(std::unordered_map<std::string, std::uint32_t>& terms,
                          std::string_view text) {
        for (auto word : SearchQuery::tokenize(text)) ++terms[std::move(word)];
    }

    SearchIndex index_;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, std::uint32_t>> postings_;
};

}  // namespace crumb::domain
