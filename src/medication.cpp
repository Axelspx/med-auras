#include "medication.h"

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

void Medication::mark_taken(const std::chrono::system_clock::time_point now) {
    last_taken_at = now;
}
