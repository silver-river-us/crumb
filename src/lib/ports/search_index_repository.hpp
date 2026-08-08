#pragma once

#include "domain/value_objects/value_objects.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace crumb::ports {
struct SearchDocument {
    domain::DirectoryPath directory;
    domain::FileName name;
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
};

class SearchIndexRepository {
public:
    virtual ~SearchIndexRepository() = default;
    virtual std::expected<void, std::string> save(const domain::DirectoryPath&, const SearchIndex&) = 0;
    virtual std::expected<SearchIndex, std::string> load(const domain::DirectoryPath&) const = 0;
};
}
