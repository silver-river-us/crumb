#pragma once
#include "files/application/clock.hpp"
namespace crumb::infrastructure {
class SystemClock final : public ports::Clock {
   public:
    std::string now_utc() override;
};
}  // namespace crumb::infrastructure
