#pragma once

#include <windows.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

enum class IntervalUnit {
    minutes,
    hours,
    days,
    weeks,
};

[[nodiscard]] std::optional<std::chrono::minutes> interval_in_minutes(double value, IntervalUnit unit);

// Local/UTC conversion. Schedules are authored in local wall-clock time and stored in UTC, so every
// occurrence calculation crosses this boundary; the display code formats through it too.
[[nodiscard]] std::chrono::system_clock::time_point system_time_to_time_point(const SYSTEMTIME& value);
[[nodiscard]] SYSTEMTIME time_point_to_system_time(std::chrono::system_clock::time_point value);
[[nodiscard]] SYSTEMTIME local_time_for(std::chrono::system_clock::time_point value);
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> utc_time_for(const SYSTEMTIME& local);

enum class ScheduleType {
    hourly,
    daily,
    weekly,
    monthly,
};

// One fixed wall-clock time in a repeating schedule. `minute` is minutes since local midnight.
// `day` is a weekday (0 = Sunday) for weekly schedules and a day of the month (1-31) for monthly
// ones; daily schedules ignore it. A monthly day that a given month does not have is simply never
// matched, which is how "skip that occurrence" falls out.
struct ScheduleEntry {
    int day{};
    int minute{};

    [[nodiscard]] friend bool operator==(const ScheduleEntry&, const ScheduleEntry&) = default;
};

enum class DoseStatus {
    taken,
    missed,
    paused,
    resumed,
};

struct DoseRecord {
    // For taken and missed this is the occurrence the record is about. For paused it is the
    // occurrence that was abandoned, and for resumed the one tracking restarted on.
    std::chrono::system_clock::time_point scheduled_at{};
    std::optional<std::chrono::system_clock::time_point> taken_at;
    DoseStatus status{DoseStatus::missed};
};

// Oldest records are dropped past this depth: over a year at one dose a day.
inline constexpr std::size_t max_history_records = 500;

struct Medication {
    std::wstring id;
    std::wstring name;
    std::wstring dose;
    std::optional<std::wstring> icon_path;
    bool enabled{true};

    ScheduleType schedule_type{ScheduleType::hourly};
    // Hourly only: repeat every `interval`, counted from `anchor_at`.
    std::chrono::minutes interval{};
    std::chrono::system_clock::time_point anchor_at{};
    // Daily, weekly, and monthly only.
    std::vector<ScheduleEntry> entries;

    // The single occurrence being tracked: the earliest one neither taken nor missed. Derived from
    // the schedule, never from when a dose was actually taken.
    std::chrono::system_clock::time_point active_at{};
    std::vector<DoseRecord> history;

    [[nodiscard]] bool schedule_is_valid() const;
    [[nodiscard]] bool is_overdue_at(std::chrono::system_clock::time_point now) const;
    [[nodiscard]] bool is_soon_at(std::chrono::system_clock::time_point now) const;

    // Recomputes the tracked occurrence from the schedule alone. Used after an edit and on resume.
    void reset_active_occurrence(std::chrono::system_clock::time_point now);
    // Records the dose that is due now as taken and everything the schedule passed before it as
    // missed, then advances to the next occurrence. Never moves later occurrences.
    void mark_taken(std::chrono::system_clock::time_point now);
    void pause();
    void resume(std::chrono::system_clock::time_point now);
    void trim_history();
};

// The whole schedule contract lives in these two. Nothing else may work out when a dose is due.
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> next_occurrence_after(
    const Medication& medication, std::chrono::system_clock::time_point after);
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> previous_occurrence_before(
    const Medication& medication, std::chrono::system_clock::time_point before);
