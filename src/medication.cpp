#include "medication.h"

#include <algorithm>
#include <cmath>
#include <limits>

std::optional<std::chrono::minutes> interval_in_minutes(const double value, const IntervalUnit unit) {
    constexpr double multipliers[]{1.0, 60.0, 1'440.0, 10'080.0};
    const double minutes = value * multipliers[static_cast<std::size_t>(unit)];
    if (!std::isfinite(minutes) || minutes < 0.5 ||
        minutes > static_cast<double>(std::numeric_limits<std::chrono::minutes::rep>::max())) {
        return std::nullopt;
    }
    return std::chrono::minutes{static_cast<std::chrono::minutes::rep>(std::llround(minutes))};
}

std::optional<std::chrono::system_clock::time_point> Medication::next_available_at() const {
    if (!last_taken_at) {
        return std::nullopt;
    }

    return *last_taken_at + interval;
}

std::chrono::minutes Medication::remaining_at(const std::chrono::system_clock::time_point now) const {
    const auto next = next_available_at();
    if (!next || *next <= now) {
        return std::chrono::minutes::zero();
    }

    return std::chrono::ceil<std::chrono::minutes>(*next - now);
}

bool Medication::is_ready_at(const std::chrono::system_clock::time_point now) const {
    return enabled && remaining_at(now) == std::chrono::minutes::zero();
}

bool Medication::is_soon_at(const std::chrono::system_clock::time_point now) const {
    const std::chrono::minutes remaining = remaining_at(now);
    return enabled && remaining > std::chrono::minutes::zero() &&
           remaining <= std::max(std::chrono::minutes{1}, interval / 10);
}

void Medication::mark_taken(const std::chrono::system_clock::time_point now) {
    last_taken_at = now;
}
