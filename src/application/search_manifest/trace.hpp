#pragma once

#include <cstdint>
#include <string>

namespace crumb::application {

struct SearchTraceSpan {
    std::string name;
    std::uint64_t offset_us{};
    std::uint64_t duration_us{};
    std::string detail;
};

}  // namespace crumb::application
