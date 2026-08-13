#pragma once

#include "domain/search_index/index.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace crumb::domain {

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
