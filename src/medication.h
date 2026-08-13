#pragma once

#include <chrono>
#include <optional>
#include <string>

enum class IntervalUnit {
    minutes,
    hours,
    days,
    weeks,
};

[[nodiscard]] std::optional<std::chrono::minutes> interval_in_minutes(double value, IntervalUnit unit);

struct Medication {
    std::wstring id;
    std::wstring name;
    std::wstring dose;
    std::optional<std::wstring> icon_path;
    std::chrono::minutes interval{};
    std::optional<std::chrono::system_clock::time_point> last_taken_at;
    bool enabled{true};

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> next_available_at() const;
    [[nodiscard]] std::chrono::minutes remaining_at(std::chrono::system_clock::time_point now) const;
    [[nodiscard]] bool is_ready_at(std::chrono::system_clock::time_point now) const;
    [[nodiscard]] bool is_soon_at(std::chrono::system_clock::time_point now) const;
    void mark_taken(std::chrono::system_clock::time_point now);
};
