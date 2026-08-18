#pragma once

#include <cstddef>

namespace crumb::application {

struct ReconcileResult {
    std::size_t scanned{};
    std::size_t added{};
    std::size_t updated{};
    std::size_t removed{};
};

}  // namespace crumb::application
