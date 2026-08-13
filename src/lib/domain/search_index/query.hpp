#pragma once

#include <algorithm>
#include <cctype>
#include <expected>
#include <string>
#include <string_view>
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

    [[nodiscard]] static bool is_documents_alias(std::string_view term) {
        return term == "docs" || term == "document" || term == "documents";
    }

    [[nodiscard]] static bool fuzzy_contains(const std::string& value, const std::string& word) {
        if (is_documents_alias(value) && is_documents_alias(word)) return true;
        if (word.size() < 3) return value == word;
        const auto prefix_length = std::max<std::size_t>(4, word.size() - 2);
        return value.find(word) != std::string::npos ||
               value.find(word.substr(0, prefix_length)) != std::string::npos;
    }

   private:
    explicit SearchQuery(std::vector<std::string> words) : words_(std::move(words)) {}

    std::vector<std::string> words_;
};

}  // namespace crumb::domain
