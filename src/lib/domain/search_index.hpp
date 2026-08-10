#pragma once

#include "domain/file_entry.hpp"
#include "domain/value_objects/value_objects.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <expected>
#include <map>

#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace crumb::domain {

class SearchQuery {
public:
    [[nodiscard]] static std::expected<SearchQuery, std::string> create(std::string_view value) {
        std::vector<std::string> words;
        std::string current;
        const auto add = [&words](std::string word) {
            if (word.size() >= 3 && std::ranges::find(words, word) == words.end()) {
                words.push_back(std::move(word));
            }
        };

        for (const unsigned char character : value) {
            if (std::isalnum(character) || character == '_') {
                current.push_back(static_cast<char>(std::tolower(character)));
            } else if (!current.empty()) {
                add(std::move(current));
                current.clear();
            }
        }
        if (!current.empty()) add(std::move(current));
        if (words.empty()) return std::unexpected("search query must contain a word with at least 3 characters");
        return SearchQuery(std::move(words));
    }

    [[nodiscard]] const std::vector<std::string>& words() const noexcept { return words_; }

    [[nodiscard]] static bool fuzzy_contains(const std::string& value, const std::string& word) {
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
        std::map<std::uint32_t, double> scores;
        const auto total = static_cast<double>(documents.size());

        for (const auto& word : query.words()) {
            std::vector<const SearchTerm*> matching_terms;
            const auto exact = std::ranges::lower_bound(terms, word, {}, [](const auto& term) {
                return term.term;
            });
            if (exact != terms.end() && exact->term == word) {
                matching_terms.push_back(&*exact);
            } else {
                for (const auto& term : terms) {
                    if (SearchQuery::fuzzy_contains(term.term, word)) matching_terms.push_back(&term);
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
                for (const auto& posting : term->postings) scores[posting.document_id] += weight * posting.count;
            }
        }

        std::vector<SearchHit> hits;
        for (const auto& [document_id, score] : scores) {
            if (score > 0.0 && document_id < documents.size()) hits.push_back({document_id, score});
        }
        return hits;
    }
};

class SearchIndexBuilder {
public:
    void add(DirectoryPath directory, const FileEntry& entry) {
        const auto document_id = static_cast<std::uint32_t>(index_.documents.size());
        index_.documents.push_back({std::move(directory), entry.name});

        std::unordered_map<std::string, std::uint32_t> terms;
        add_terms(terms, entry.name.value());
        add_terms(terms, entry.metadata.type);
        if (entry.metadata.title) add_terms(terms, *entry.metadata.title);
        if (entry.metadata.author) add_terms(terms, *entry.metadata.author);
        for (const auto& tag : entry.metadata.tags) add_terms(terms, tag);
        for (const auto& [key, value] : entry.metadata.extension_fields) add_terms(terms, key + " " + value);
        for (const auto& [term, count] : terms) postings_[term][document_id] = count;
    }

    [[nodiscard]] SearchIndex build() && {
        for (auto& [term, postings] : postings_) {
            SearchTerm item{std::move(term), {}};
            for (const auto& [document_id, count] : postings) item.postings.push_back({document_id, count});
            std::ranges::sort(item.postings, {}, &SearchPosting::document_id);
            index_.terms.push_back(std::move(item));
        }
        std::ranges::sort(index_.terms, {}, &SearchTerm::term);
        return std::move(index_);
    }

private:
    static void add_terms(std::unordered_map<std::string, std::uint32_t>& terms, std::string_view text) {
        std::string current;
        const auto add = [&terms](std::string word) {
            if (word.size() >= 3) ++terms[word];
        };

        for (const unsigned char character : text) {
            if (std::isalnum(character) || character == '_') {
                current.push_back(static_cast<char>(std::tolower(character)));
            } else if (!current.empty()) {
                add(std::move(current));
                current.clear();
            }
        }
        if (!current.empty()) add(std::move(current));
    }

    SearchIndex index_;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, std::uint32_t>> postings_;
};

} // namespace crumb::domain
