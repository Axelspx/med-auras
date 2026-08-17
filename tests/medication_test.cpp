#include "medication.h"
#include "storage.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(condition)                                                                                              \
    do {                                                                                                              \
        if (!(condition)) {                                                                                           \
            std::cerr << "Check failed at line " << __LINE__ << ": " #condition "\n";                              \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (false)

namespace {

using namespace std::chrono_literals;

// Fixed schedule times are local wall-clock, so the expected values in these checks are built the
// same way the scheduler builds them. What is under test is which day and which entry it picks.
std::chrono::system_clock::time_point local(
    const int year, const unsigned month, const unsigned day, const int hour, const int minute) {
    const SYSTEMTIME value{
        .wYear = static_cast<WORD>(year),
        .wMonth = static_cast<WORD>(month),
        .wDay = static_cast<WORD>(day),
        .wHour = static_cast<WORD>(hour),
        .wMinute = static_cast<WORD>(minute),
    };
    return utc_time_for(value).value();
}

std::chrono::system_clock::time_point utc(
    const int year, const unsigned month, const unsigned day, const int hour, const int minute) {
    return std::chrono::sys_days{std::chrono::year{year} / month / day} + std::chrono::hours{hour} +
           std::chrono::minutes{minute};
}

Medication daily_medication() {
    return Medication{
        .id = L"daily",
        .name = L"Daily medication",
        .dose = L"40 mg",
        .schedule_type = ScheduleType::daily,
        .entries = {{.minute = 510}, {.minute = 750}, {.minute = 990}},
    };
}

}

int main() {
    CHECK(interval_in_minutes(3.5, IntervalUnit::days) == 5'040min);
    CHECK(interval_in_minutes(2.0, IntervalUnit::weeks) == 20'160min);
    CHECK(!interval_in_minutes(0.0, IntervalUnit::hours));

    // Hourly repeats continuously from its anchor and does not restart each day.
    Medication hourly{
        .id = L"hourly",
        .name = L"Hourly medication",
        .schedule_type = ScheduleType::hourly,
        .interval = 4h,
        .anchor_at = utc(2026, 8, 16, 8, 30),
    };
    CHECK(next_occurrence_after(hourly, utc(2026, 8, 16, 8, 30)) == utc(2026, 8, 16, 12, 30));
    CHECK(next_occurrence_after(hourly, utc(2026, 8, 16, 12, 29)) == utc(2026, 8, 16, 12, 30));
    CHECK(next_occurrence_after(hourly, utc(2026, 8, 16, 20, 30)) == utc(2026, 8, 17, 0, 30));
    CHECK(previous_occurrence_before(hourly, utc(2026, 8, 16, 8, 30)) == utc(2026, 8, 16, 4, 30));
    CHECK(previous_occurrence_before(hourly, utc(2026, 8, 16, 12, 31)) == utc(2026, 8, 16, 12, 30));

    // An interval that does not divide 24 walks across the day rather than snapping back.
    hourly.interval = 5h;
    hourly.anchor_at = utc(2026, 8, 16, 8, 0);
    CHECK(next_occurrence_after(hourly, utc(2026, 8, 16, 20, 0)) == utc(2026, 8, 16, 23, 0));
    CHECK(next_occurrence_after(hourly, utc(2026, 8, 16, 23, 0)) == utc(2026, 8, 17, 4, 0));

    Medication daily = daily_medication();
    daily.reset_active_occurrence(local(2026, 8, 16, 7, 0));
    CHECK(daily.active_at == local(2026, 8, 16, 8, 30));
    CHECK(next_occurrence_after(daily, local(2026, 8, 16, 16, 30)) == local(2026, 8, 17, 8, 30));
    CHECK(previous_occurrence_before(daily, local(2026, 8, 17, 8, 30)) == local(2026, 8, 16, 16, 30));

    // 2026-08-16 is a Sunday.
    const Medication weekly{
        .id = L"weekly",
        .schedule_type = ScheduleType::weekly,
        .entries = {{.day = 3, .minute = 720}, {.day = 0, .minute = 0}},
    };
    CHECK(next_occurrence_after(weekly, local(2026, 8, 16, 1, 0)) == local(2026, 8, 19, 12, 0));
    CHECK(previous_occurrence_before(weekly, local(2026, 8, 16, 1, 0)) == local(2026, 8, 16, 0, 0));
    CHECK(next_occurrence_after(weekly, local(2026, 8, 19, 12, 0)) == local(2026, 8, 23, 0, 0));

    // A month without the selected day skips the occurrence: February has no 31st.
    const Medication monthly{
        .id = L"monthly",
        .schedule_type = ScheduleType::monthly,
        .entries = {{.day = 31, .minute = 540}},
    };
    CHECK(next_occurrence_after(monthly, local(2026, 1, 31, 10, 0)) == local(2026, 3, 31, 9, 0));

    // Pressing Taken late records the dose that was due, not the moment it was pressed, and does
    // not move the next one.
    daily = daily_medication();
    daily.active_at = local(2026, 8, 16, 12, 30);
    daily.mark_taken(local(2026, 8, 16, 12, 47));
    CHECK(daily.history.size() == 1);
    CHECK(daily.history[0].status == DoseStatus::taken);
    CHECK(daily.history[0].scheduled_at == local(2026, 8, 16, 12, 30));
    CHECK(daily.history[0].taken_at == local(2026, 8, 16, 12, 47));
    CHECK(daily.active_at == local(2026, 8, 16, 16, 30));

    // Pressing early resolves the upcoming dose and still leaves the schedule where it was.
    daily = daily_medication();
    daily.active_at = local(2026, 8, 16, 16, 30);
    daily.mark_taken(local(2026, 8, 16, 16, 20));
    CHECK(daily.history.size() == 1);
    CHECK(daily.history[0].taken_at == local(2026, 8, 16, 16, 20));
    CHECK(daily.active_at == local(2026, 8, 17, 8, 30));

    // Three days away: one press records the dose due now and marks the rest missed. It never
    // leaves another past occurrence active.
    daily = daily_medication();
    daily.active_at = local(2026, 8, 16, 8, 30);
    daily.mark_taken(local(2026, 8, 19, 13, 0));
    CHECK(daily.history.size() == 11);
    CHECK(std::ranges::count_if(daily.history, [](const DoseRecord& record) {
              return record.status == DoseStatus::missed;
          }) == 10);
    CHECK(daily.history.front().scheduled_at == local(2026, 8, 16, 8, 30));
    CHECK(daily.history[9].scheduled_at == local(2026, 8, 19, 8, 30));
    CHECK(daily.history.back().status == DoseStatus::taken);
    CHECK(daily.history.back().scheduled_at == local(2026, 8, 19, 12, 30));
    CHECK(daily.history.back().taken_at == local(2026, 8, 19, 13, 0));
    CHECK(daily.active_at == local(2026, 8, 19, 16, 30));

    CHECK(daily.is_overdue_at(local(2026, 8, 19, 16, 35)));
    CHECK(!daily.is_overdue_at(local(2026, 8, 19, 16, 25)));
    CHECK(daily.is_soon_at(local(2026, 8, 19, 16, 20)));
    CHECK(!daily.is_soon_at(local(2026, 8, 19, 15, 0)));

    // Pausing accrues nothing: resuming days later lands on the next future dose with no misses.
    const std::size_t before_pause = daily.history.size();
    daily.pause();
    CHECK(!daily.enabled);
    CHECK(daily.history.back().status == DoseStatus::paused);
    CHECK(!daily.is_overdue_at(local(2026, 8, 25, 9, 0)));
    daily.resume(local(2026, 8, 25, 9, 0));
    CHECK(daily.enabled);
    CHECK(daily.active_at == local(2026, 8, 25, 12, 30));
    CHECK(daily.history.size() == before_pause + 2);
    CHECK(daily.history.back().status == DoseStatus::resumed);

    Medication trimmed = daily_medication();
    trimmed.history.assign(max_history_records + 100, DoseRecord{.scheduled_at = utc(2026, 8, 16, 8, 30)});
    trimmed.history.front().status = DoseStatus::taken;
    trimmed.trim_history();
    CHECK(trimmed.history.size() == max_history_records);
    CHECK(trimmed.history.front().status == DoseStatus::missed);

    const Medication no_entries{.schedule_type = ScheduleType::daily};
    const Medication no_interval{.schedule_type = ScheduleType::hourly};
    const Medication bad_weekday{.schedule_type = ScheduleType::weekly, .entries = {{.day = 7, .minute = 0}}};
    const Medication bad_day{.schedule_type = ScheduleType::monthly, .entries = {{.day = 0, .minute = 0}}};
    CHECK(!no_entries.schedule_is_valid());
    CHECK(!no_interval.schedule_is_valid());
    CHECK(!bad_weekday.schedule_is_valid());
    CHECK(!bad_day.schedule_is_valid());

    const std::filesystem::path json_path = std::filesystem::temp_directory_path() / "med-auras-storage-test.json";
    std::filesystem::remove(json_path);
    WidgetSettings settings;
    CHECK(load_medications(json_path, &settings).empty());
    CHECK(!settings.window_x);
    CHECK(!settings.window_y);

    Medication stored = daily_medication();
    stored.icon_path = L"icons\\morning.png";
    stored.name = L"Café \"morning\" medication";
    stored.active_at = utc(2026, 8, 13, 10, 30);
    stored.history.push_back({.scheduled_at = utc(2026, 8, 12, 10, 30), .status = DoseStatus::missed});
    stored.history.push_back(
        {.scheduled_at = utc(2026, 8, 13, 6, 30),
         .taken_at = utc(2026, 8, 13, 6, 41),
         .status = DoseStatus::taken});
    Medication second{
        .id = L"evening-medication",
        .name = L"Evening medication",
        .dose = L"10 mg",
        .schedule_type = ScheduleType::hourly,
        .interval = 24h,
        .anchor_at = utc(2026, 8, 13, 20, 0),
        .active_at = utc(2026, 8, 14, 20, 0),
    };
    settings = {
        .window_x = -200,
        .window_y = 150,
        .position_locked = true,
        .always_on_top = true,
    };
    save_medications(json_path, std::vector{stored, second}, settings);

    WidgetSettings loaded_settings;
    const std::vector<Medication> loaded = load_medications(json_path, &loaded_settings);
    CHECK(loaded.size() == 2);
    CHECK(loaded[0].id == stored.id);
    CHECK(loaded[0].name == stored.name);
    CHECK(loaded[0].dose == stored.dose);
    CHECK(loaded[0].icon_path == stored.icon_path);
    CHECK(loaded[0].schedule_type == ScheduleType::daily);
    CHECK(loaded[0].entries == stored.entries);
    CHECK(loaded[0].active_at == stored.active_at);
    CHECK(loaded[0].enabled == stored.enabled);
    CHECK(loaded[0].history.size() == 2);
    CHECK(loaded[0].history[0].status == DoseStatus::missed);
    CHECK(!loaded[0].history[0].taken_at);
    CHECK(loaded[0].history[1].status == DoseStatus::taken);
    CHECK(loaded[0].history[1].taken_at == stored.history[1].taken_at);
    CHECK(loaded[1].id == second.id);
    CHECK(!loaded[1].icon_path);
    CHECK(loaded[1].schedule_type == ScheduleType::hourly);
    CHECK(loaded[1].interval == 24h);
    CHECK(loaded[1].anchor_at == second.anchor_at);
    CHECK(loaded[1].history.empty());
    CHECK(loaded_settings.window_x == settings.window_x);
    CHECK(loaded_settings.window_y == settings.window_y);
    CHECK(loaded_settings.position_locked);
    CHECK(loaded_settings.always_on_top);

    std::ifstream json(json_path);
    const std::string text{std::istreambuf_iterator<char>{json}, std::istreambuf_iterator<char>{}};
    json.close();
    CHECK(text.find("2026-08-13T10:30:00Z") != std::string::npos);
    CHECK(text.find("remaining") == std::string::npos);

    stored.dose = L"50 mg";
    save_medications(json_path, std::vector{stored, second}, settings);
    CHECK(load_medications(json_path)[0].dose == L"50 mg");
    CHECK(!std::filesystem::exists(json_path.wstring() + L".tmp"));

    const std::filesystem::path temporary_path = json_path.wstring() + L".tmp";
    std::filesystem::create_directory(temporary_path);
    stored.dose = L"60 mg";
    bool save_failed{};
    try {
        save_medications(json_path, std::vector{stored, second}, settings);
    } catch (const std::exception&) {
        save_failed = true;
    }
    CHECK(save_failed);
    CHECK(load_medications(json_path)[0].dose == L"50 mg");
    std::filesystem::remove(temporary_path);

    const std::filesystem::path corrupt_path = json_path.wstring() + L".corrupt";
    std::ofstream corrupt(corrupt_path, std::ios::trunc);
    corrupt << "{\"medications\": [";
    corrupt.close();
    bool corrupt_rejected{};
    try {
        static_cast<void>(load_medications(corrupt_path));
    } catch (const std::exception&) {
        corrupt_rejected = true;
    }
    CHECK(corrupt_rejected);
    std::filesystem::remove(corrupt_path);

    // A file written before the schedule system: the free-running interval becomes an hourly
    // schedule anchored at the last dose, and the dose that was pending stays pending.
    std::ofstream legacy_file(json_path, std::ios::trunc);
    legacy_file << R"({"medications": [{"id": "a", "name": "Legacy", "dose": "40 mg", "icon_path": null,)"
                << R"( "interval_minutes": 720, "last_taken_at": "2026-08-13T10:30:00Z", "enabled": true},)"
                << R"( {"id": "b", "name": "Never taken", "dose": "", "icon_path": null,)"
                << R"( "interval_minutes": 60, "last_taken_at": null, "enabled": true}]})" << '\n';
    legacy_file.close();
    const std::vector<Medication> migrated = load_medications(json_path);
    CHECK(migrated.size() == 2);
    CHECK(migrated[0].schedule_type == ScheduleType::hourly);
    CHECK(migrated[0].interval == 720min);
    CHECK(migrated[0].anchor_at == utc(2026, 8, 13, 10, 30));
    CHECK(migrated[0].active_at == utc(2026, 8, 13, 22, 30));
    CHECK(migrated[0].history.empty());
    CHECK(migrated[1].interval == 60min);
    const auto never_taken_delay = migrated[1].active_at - std::chrono::system_clock::now();
    CHECK(never_taken_delay < 5s && never_taken_delay > -5s);

    std::ofstream legacy(json_path, std::ios::trunc);
    legacy << "{\"medications\": []}\n";
    legacy.close();
    loaded_settings = settings;
    CHECK(load_medications(json_path, &loaded_settings).empty());
    CHECK(!loaded_settings.window_x);
    CHECK(!loaded_settings.window_y);
    CHECK(!loaded_settings.position_locked);
    CHECK(!loaded_settings.always_on_top);

    // Files written by the withdrawn Mica/Acrylic build still carry "background_material".
    // The key is now unknown and must be ignored rather than rejected.
    std::ofstream retired_settings(json_path, std::ios::trunc);
    retired_settings << R"({"medications": [], "settings": {"window_x": 12, "window_y": 34, "position_locked": true, "always_on_top": true, "background_material": "acrylic"}})" << '\n';
    retired_settings.close();
    loaded_settings = {};
    CHECK(load_medications(json_path, &loaded_settings).empty());
    CHECK(loaded_settings.window_x == 12);
    CHECK(loaded_settings.window_y == 34);
    CHECK(loaded_settings.position_locked);
    CHECK(loaded_settings.always_on_top);
    std::filesystem::remove(json_path);
}
