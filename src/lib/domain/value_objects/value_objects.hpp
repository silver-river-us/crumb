#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace crumb::domain {

class ValidatedString {
public:
    ValidatedString() = default;
    explicit ValidatedString(std::string value) : value_(std::move(value)) {}
    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
    auto operator<=>(const ValidatedString&) const = default;

protected:
    std::string value_;
};

class FileName : public ValidatedString {
public:
    using ValidatedString::ValidatedString;
    static FileName create(std::string value) {
        if (value.empty() || value == "." || value == ".." || value.find('/') != std::string::npos ||
            value.find('\\') != std::string::npos) {
            throw std::invalid_argument("invalid immediate filename");
        }
        return FileName(std::move(value));
    }
    using ValidatedString::operator<=>;
};

class DirectoryPath : public ValidatedString {
public:
    using ValidatedString::ValidatedString;
    static DirectoryPath create(std::string value) {
        if (value.empty()) throw std::invalid_argument("directory path must not be empty");
        return DirectoryPath(std::move(value));
    }
    using ValidatedString::operator<=>;
};

class DirectoryId : public ValidatedString {
public:
    using ValidatedString::ValidatedString;
    static DirectoryId create(std::string value) {
        if (value.size() != 26) throw std::invalid_argument("directory id must be a ULID");
        return DirectoryId(std::move(value));
    }
    using ValidatedString::operator<=>;
};

class FileId : public ValidatedString {
public:
    using ValidatedString::ValidatedString;
    static FileId create(std::string value) {
        if (value.size() != 26) throw std::invalid_argument("file id must be a ULID");
        return FileId(std::move(value));
    }
    using ValidatedString::operator<=>;
};

inline std::string file_id_hash(const FileId& id) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char character : id.value()) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    std::string result = "fid:";
    for (int shift = 60; shift >= 0; shift -= 4) {
        result += hex[(hash >> shift) & 0x0f];
    }
    return result;
}

class Fingerprint : public ValidatedString {
public:
    using ValidatedString::ValidatedString;
    static Fingerprint create(std::string value) {
        if (value.find(':') == std::string::npos) throw std::invalid_argument("fingerprint needs an algorithm");
        return Fingerprint(std::move(value));
    }
    using ValidatedString::operator<=>;
};

class ContentHash : public ValidatedString {
public:
    using ValidatedString::ValidatedString;
    static ContentHash create(std::string value) {
        if (value.find(':') == std::string::npos) throw std::invalid_argument("content hash needs an algorithm");
        return ContentHash(std::move(value));
    }
    using ValidatedString::operator<=>;
};

} // namespace crumb::domain
