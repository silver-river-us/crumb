#pragma once

#include "files/domain/value_objects/file_id.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace crumb::domain {

inline std::string file_id_hash(const FileId& id) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::uint64_t hash = 14695981039346656037ull;
    for (const char character : id.value()) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ull;
    }
    std::string result = "fid:";
    for (int shift = 60; shift >= 0; shift -= 4) {
        result += hex[(hash >> shift) & 0x0f];
    }
    return result;
}

}  // namespace crumb::domain
