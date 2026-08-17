#include "medication.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// A monthly entry on the 31st has to look at most two months ahead to find a month that has one.
constexpr int scan_days = 400;

long long floor_divide(const long long value, const long long divisor) {
    const long long quotient = value / divisor;
    return value % divisor != 0 && (value < 0) != (divisor < 0) ? quotient - 1 : quotient;
}

std::chrono::year_month_day local_date_of(const std::chrono::system_clock::time_point value) {
    const SYSTEMTIME local = local_time_for(value);
    return std::chrono::year_month_day{
        std::chrono::year{local.wYear}, std::chrono::month{local.wMonth}, std::chrono::day{local.wDay}};
}

// Every occurrence the schedule places on one local date, in order. Converting to UTC here is what
// keeps a fixed wall-clock time fixed across a daylight-saving change.
void day_occurrences(
    const Medication& medication, const std::chrono::year_month_day date,
    std::vector<std::chrono::system_clock::time_point>& occurrences) {
    occurrences.clear();
    const unsigned weekday = std::chrono::weekday{std::chrono::sys_days{date}}.c_encoding();
    for (const ScheduleEntry& entry : medication.entries) {
        if (medication.schedule_type == ScheduleType::weekly && entry.day != static_cast<int>(weekday)) continue;
        if (medication.schedule_type == ScheduleType::monthly &&
            entry.day != static_cast<int>(static_cast<unsigned>(date.day()))) {
            continue;
        }
        const SYSTEMTIME local{
            .wYear = static_cast<WORD>(static_cast<int>(date.year())),
            .wMonth = static_cast<WORD>(static_cast<unsigned>(date.month())),
            .wDay = static_cast<WORD>(static_cast<unsigned>(date.day())),
            .wHour = static_cast<WORD>(entry.minute / 60),
            .wMinute = static_cast<WORD>(entry.minute % 60),
        };
        if (const auto utc = utc_time_for(local)) occurrences.push_back(*utc);
    }
    std::ranges::sort(occurrences);
}

std::optional<std::chrono::system_clock::time_point> hourly_occurrence(
    const Medication& medication, const std::chrono::system_clock::time_point cursor, const bool forward) {
    const auto step = std::chrono::duration_cast<std::chrono::seconds>(medication.interval);
    if (step <= std::chrono::seconds::zero()) return std::nullopt;

    // The anchor is a start time, not a floor: multiples before it exist so a card tracking the very
    // first dose still has a previous occurrence to measure its progress bar against.
    const auto elapsed = std::chrono::floor<std::chrono::seconds>(cursor - medication.anchor_at);
    long long steps = floor_divide(elapsed.count(), step.count());
    const auto at = [&](const long long count) {
        return medication.anchor_at + std::chrono::seconds{count * step.count()};
    };
    if (forward) {
        while (at(steps) <= cursor) ++steps;
    } else {
        while (at(steps) >= cursor) --steps;
    }
    return at(steps);
}

std::optional<std::chrono::system_clock::time_point> listed_occurrence(
    const Medication& medication, const std::chrono::system_clock::time_point cursor, const bool forward) {
    if (medication.entries.empty()) return std::nullopt;

    const auto start = std::chrono::sys_days{local_date_of(cursor)};
    std::vector<std::chrono::system_clock::time_point> occurrences;
    for (int offset = 0; offset <= scan_days; ++offset) {
        const auto day = forward ? start + std::chrono::days{offset} : start - std::chrono::days{offset};
        day_occurrences(medication, std::chrono::year_month_day{day}, occurrences);
        if (forward) {
            for (const auto occurrence : occurrences) {
                if (occurrence > cursor) return occurrence;
            }
        } else {
            for (auto it = occurrences.rbegin(); it != occurrences.rend(); ++it) {
                if (*it < cursor) return *it;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::chrono::system_clock::time_point> occurrence(
    const Medication& medication, const std::chrono::system_clock::time_point cursor, const bool forward) {
    if (!medication.schedule_is_valid()) return std::nullopt;
    return medication.schedule_type == ScheduleType::hourly ? hourly_occurrence(medication, cursor, forward)
                                                            : listed_occurrence(medication, cursor, forward);
}

}

std::optional<std::chrono::minutes> interval_in_minutes(const double value, const IntervalUnit unit) {
    constexpr double multipliers[]{1.0, 60.0, 1'440.0, 10'080.0};
    const double minutes = value * multipliers[static_cast<std::size_t>(unit)];
    if (!std::isfinite(minutes) || minutes < 0.5 ||
        minutes > static_cast<double>(std::numeric_limits<std::chrono::minutes::rep>::max())) {
        return std::nullopt;
    }
    return std::chrono::minutes{static_cast<std::chrono::minutes::rep>(std::llround(minutes))};
}

std::chrono::system_clock::time_point system_time_to_time_point(const SYSTEMTIME& value) {
    const std::chrono::year_month_day date{
        std::chrono::year{value.wYear}, std::chrono::month{value.wMonth}, std::chrono::day{value.wDay}};
    return std::chrono::sys_days{date} + std::chrono::hours{value.wHour} + std::chrono::minutes{value.wMinute} +
           std::chrono::seconds{value.wSecond};
}

SYSTEMTIME time_point_to_system_time(const std::chrono::system_clock::time_point value) {
    const auto seconds = std::chrono::floor<std::chrono::seconds>(value);
    const auto day = std::chrono::floor<std::chrono::days>(seconds);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{seconds - day};
    return SYSTEMTIME{
        .wYear = static_cast<WORD>(static_cast<int>(date.year())),
        .wMonth = static_cast<WORD>(static_cast<unsigned>(date.month())),
        .wDayOfWeek = static_cast<WORD>(std::chrono::weekday{day}.c_encoding()),
        .wDay = static_cast<WORD>(static_cast<unsigned>(date.day())),
        .wHour = static_cast<WORD>(time.hours().count()),
        .wMinute = static_cast<WORD>(time.minutes().count()),
        .wSecond = static_cast<WORD>(time.seconds().count()),
    };
}

SYSTEMTIME local_time_for(const std::chrono::system_clock::time_point value) {
    const SYSTEMTIME utc = time_point_to_system_time(value);
    SYSTEMTIME local{};
    return SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local) ? local : utc;
}

std::optional<std::chrono::system_clock::time_point> utc_time_for(const SYSTEMTIME& local) {
    SYSTEMTIME utc{};
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &local, &utc)) return std::nullopt;
    return system_time_to_time_point(utc);
}

std::optional<std::chrono::system_clock::time_point> next_occurrence_after(
    const Medication& medication, const std::chrono::system_clock::time_point after) {
    return occurrence(medication, after, true);
}

std::optional<std::chrono::system_clock::time_point> previous_occurrence_before(
    const Medication& medication, const std::chrono::system_clock::time_point before) {
    return occurrence(medication, before, false);
}

bool Medication::schedule_is_valid() const {
    if (schedule_type == ScheduleType::hourly) return interval > std::chrono::minutes::zero();
    if (entries.empty()) return false;
    return std::ranges::all_of(entries, [this](const ScheduleEntry& entry) {
        if (entry.minute < 0 || entry.minute > 1'439) return false;
        if (schedule_type == ScheduleType::weekly) return entry.day >= 0 && entry.day <= 6;
        if (schedule_type == ScheduleType::monthly) return entry.day >= 1 && entry.day <= 31;
        return true;
    });
}

bool Medication::is_overdue_at(const std::chrono::system_clock::time_point now) const {
    return enabled && now >= active_at;
}

bool Medication::is_soon_at(const std::chrono::system_clock::time_point now) const {
    if (!enabled || now >= active_at) return false;
    using Duration = std::chrono::system_clock::duration;
    const auto previous = previous_occurrence_before(*this, active_at);
    const Duration span = previous ? Duration{active_at - *previous} : Duration{std::chrono::hours{24}};
    return Duration{active_at - now} <= std::max<Duration>(span / 10, std::chrono::minutes{1});
}

void Medication::reset_active_occurrence(const std::chrono::system_clock::time_point now) {
    if (const auto next = next_occurrence_after(*this, now)) active_at = *next;
}

void Medication::mark_taken(const std::chrono::system_clock::time_point now) {
    if (!schedule_is_valid()) return;

    // Doses the schedule passed while nobody was pressing anything are missed, not owed: the one
    // recorded as taken is the one that was due when the button was pressed.
    while (const auto next = next_occurrence_after(*this, active_at)) {
        if (*next > now) break;
        history.push_back({.scheduled_at = active_at, .status = DoseStatus::missed});
        active_at = *next;
    }

    history.push_back({.scheduled_at = active_at, .taken_at = now, .status = DoseStatus::taken});
    if (const auto next = next_occurrence_after(*this, active_at)) active_at = *next;
    trim_history();
}

void Medication::pause() {
    if (!enabled) return;
    enabled = false;
    history.push_back({.scheduled_at = active_at, .status = DoseStatus::paused});
    trim_history();
}

void Medication::resume(const std::chrono::system_clock::time_point now) {
    if (enabled) return;
    enabled = true;
    // Nothing was missed while paused, so tracking restarts at the next occurrence rather than
    // resolving everything the schedule passed in the meantime.
    reset_active_occurrence(now);
    history.push_back({.scheduled_at = active_at, .status = DoseStatus::resumed});
    trim_history();
}

void Medication::trim_history() {
    if (history.size() <= max_history_records) return;
    history.erase(history.begin(), history.end() - static_cast<std::ptrdiff_t>(max_history_records));
}
