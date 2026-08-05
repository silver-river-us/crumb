#include "infrastructure/system/ulid_generator.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace crumb::infrastructure {
UlidGenerator::UlidGenerator() : random_(std::random_device{}()) {}
std::string UlidGenerator::generate() {
    constexpr std::string_view alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    std::array<unsigned char, 16> bytes{};
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for (int i = 5; i >= 0; --i) bytes[5 - i] = static_cast<unsigned char>((milliseconds >> (i * 8)) & 0xff);
    for (std::size_t i = 6; i < bytes.size(); i += 8) {
        auto value = random_();
        for (int j = 0; j < 8 && i + j < bytes.size(); ++j) bytes[i + j] = static_cast<unsigned char>(value >> ((7 - j) * 8));
    }
    std::string result; result.reserve(26);
    for (int bit = 0; bit < 130; bit += 5) {
        unsigned value = 0;
        for (int offset = 0; offset < 5; ++offset) {
            const int position = bit + offset - 2;
            value = (value << 1) | (position >= 0 && ((bytes[position / 8] >> (7 - position % 8)) & 1));
        }
        result += alphabet[value & 31];
    }
    return result;
}
domain::DirectoryId UlidGenerator::directory_id() { return domain::DirectoryId::create(generate()); }
domain::FileId UlidGenerator::file_id() { return domain::FileId::create(generate()); }
}
