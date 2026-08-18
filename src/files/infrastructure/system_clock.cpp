#include "files/infrastructure/system_clock.hpp"
#include <chrono>
#include <ctime>
namespace crumb::infrastructure {
std::string SystemClock::now_utc() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y_%m_%dT%H:%M:%SZ", &utc);
    return buffer;
}
}  // namespace crumb::infrastructure
