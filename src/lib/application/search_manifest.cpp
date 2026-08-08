#include "lib/application/search_manifest.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace crumb::application {
namespace {
struct Document {
    domain::DirectoryPath directory;
    domain::FileName name;
    std::string type;
    std::optional<std::string> title;
    std::optional<std::string> author;
    std::vector<std::string> tags;
    std::map<std::string, std::string> extension_fields;
};
std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) result.push_back(static_cast<char>(std::tolower(character)));
    return result;
}
std::vector<std::string> query_words(std::string_view query) {
    std::vector<std::string> words;
    std::string current;
    const auto add = [&words](std::string word) {
        if (word.size() >= 3 && std::ranges::find(words, word) == words.end()) words.push_back(std::move(word));
    };
    for (const unsigned char character : query) {
        if (std::isalnum(character) || character == '_') current.push_back(static_cast<char>(std::tolower(character)));
        else if (!current.empty()) { add(std::move(current)); current.clear(); }
    }
    if (!current.empty()) add(std::move(current));
    return words;
}
std::string haystack(const Document& document) {
    std::string result = document.name.value() + " " + document.type;
    if (document.title) result += " " + *document.title;
    if (document.author) result += " " + *document.author;
    for (const auto& tag : document.tags) result += " " + tag;
    for (const auto& [key, value] : document.extension_fields) result += " " + key + " " + value;
    return lower(result);
}
bool fuzzy_contains(const std::string& value, const std::string& word) {
    const auto prefix_length = std::max<std::size_t>(4, word.size() - 2);
    return value.find(word) != std::string::npos || value.find(word.substr(0, prefix_length)) != std::string::npos;
}
}

std::expected<SearchResult, std::string> SearchManifest::execute(
    const domain::DirectoryPath& directory, std::string_view query, std::size_t limit) const {
    const auto words = query_words(query);
    if (words.empty()) return std::unexpected("search query must contain a word with at least 3 characters");

    // The persisted index is the fast path. A missing index deliberately falls back
    // to the manifest path so the library remains useful before the first scan.
    if (index_) {
        auto loaded_index = index_->load(directory);
        if (loaded_index) {
            SearchResult result;
            result.inspected = loaded_index->documents.size();
            std::map<std::uint32_t, double> scores;
            const auto total = static_cast<double>(loaded_index->documents.size());
            for (const auto& word : words) {
                std::vector<const ports::SearchTerm*> matching_terms;
                const auto exact = std::ranges::lower_bound(loaded_index->terms, word,
                    {}, [](const auto& term) { return term.term; });
                if (exact != loaded_index->terms.end() && exact->term == word) {
                    matching_terms.push_back(&*exact);
                } else {
                    for (const auto& term : loaded_index->terms)
                        if (fuzzy_contains(term.term, word)) matching_terms.push_back(&term);
                }
                std::set<std::uint32_t> matched;
                for (const auto* term : matching_terms)
                    for (const auto& posting : term->postings) matched.insert(posting.document_id);
                const auto weight = matched.empty() ? 0.0 : std::log((total + 1.0) / static_cast<double>(matched.size()));
                for (const auto* term : matching_terms)
                    for (const auto& posting : term->postings) scores[posting.document_id] += weight * posting.count;
            }
            for (const auto& [document_id, score] : scores) {
                if (score <= 0.0 || document_id >= loaded_index->documents.size()) continue;
                const auto& location = loaded_index->documents[document_id];
                result.matches.push_back({location.directory, location.name, score, {}, std::nullopt, std::nullopt});
            }
            std::ranges::sort(result.matches, [](const auto& left, const auto& right) {
                if (left.score != right.score) return left.score > right.score;
                if (left.directory.value() != right.directory.value()) return left.directory.value() < right.directory.value();
                return left.name.value() < right.name.value();
            });
            if (result.matches.size() > limit) result.matches.resize(limit);
            return result;
        }
    }

    auto directories = filesystem_.list_directories_recursive(directory);
    if (!directories) return std::unexpected(directories.error());
    std::vector<Document> documents;
    for (const auto& current : directories.value()) {
        auto loaded = manifests_.load(current);
        if (!loaded) return std::unexpected(loaded.error());
        if (!loaded.value()) continue;
        for (const auto& entry : loaded.value()->files())
            documents.push_back({current, entry.name, entry.metadata.type, entry.metadata.title, entry.metadata.author,
                                 entry.metadata.tags, entry.metadata.extension_fields});
    }
    SearchResult result;
    result.inspected = documents.size();
    std::vector<std::string> haystacks;
    for (const auto& document : documents) haystacks.push_back(haystack(document));
    const auto total = static_cast<double>(haystacks.size());
    std::vector<double> weights;
    for (const auto& word : words) {
        const auto count = static_cast<double>(std::count_if(haystacks.begin(), haystacks.end(),
            [&](const auto& value) { return fuzzy_contains(value, word); }));
        weights.push_back(count ? std::log((total + 1.0) / count) : 0.0);
    }
    for (std::size_t i = 0; i < documents.size(); ++i) {
        double score = 0;
        for (std::size_t j = 0; j < words.size(); ++j) if (fuzzy_contains(haystacks[i], words[j])) score += weights[j];
        if (score > 0) result.matches.push_back({documents[i].directory, documents[i].name, score, documents[i].type,
                                                   documents[i].title, documents[i].author});
    }
    std::ranges::sort(result.matches, [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        if (left.directory.value() != right.directory.value()) return left.directory.value() < right.directory.value();
        return left.name.value() < right.name.value();
    });
    if (result.matches.size() > limit) result.matches.resize(limit);
    return result;
}
}
