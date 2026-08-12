#pragma once

#include <string>
namespace crumb::ports {
class Clock {
   public:
    virtual ~Clock() = default;
    virtual std::string now_utc() = 0;
};
}  // namespace crumb::ports
