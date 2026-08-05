#pragma once
#include "lib/ports/clock.hpp"
namespace crumb::infrastructure { class SystemClock final : public ports::Clock { public: std::string now_utc() override; }; }
