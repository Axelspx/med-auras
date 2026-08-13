#include "medication.h"
#include "storage.h"

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

int main() {
    using namespace std::chrono_literals;

    CHECK(interval_in_minutes(3.5, IntervalUnit::days) == 5'040min);
    CHECK(interval_in_minutes(2.0, IntervalUnit::weeks) == 20'160min);
    CHECK(!interval_in_minutes(0.0, IntervalUnit::hours));

    Medication medication{
        .id = L"morning-medication",
        .name = L"Example medication",
        .dose = L"40 mg",
        .interval = 12h,
    };
    const auto now = std::chrono::system_clock::time_point{24h};

    CHECK(medication.is_ready_at(now));
    CHECK(!medication.next_available_at());

    medication.mark_taken(now);
    CHECK(medication.next_available_at() == now + 12h);
    CHECK(medication.remaining_at(now + 11h) == 60min);
    CHECK(medication.is_ready_at(now + 12h));

    medication.enabled = false;
    CHECK(!medication.is_ready_at(now + 12h));

    const std::filesystem::path json_path = std::filesystem::temp_directory_path() / "med-auras-storage-test.json";
    std::filesystem::remove(json_path);
    WidgetSettings settings;
    CHECK(load_medications(json_path, &settings).empty());
    CHECK(!settings.window_x);
    CHECK(!settings.window_y);

    medication.icon_path = L"icons\\morning.png";
    medication.name = L"Café \"morning\" medication";
    medication.last_taken_at = std::chrono::sys_days{std::chrono::year{2026} / 8 / 13} + 10h + 30min;
    Medication second{
        .id = L"evening-medication",
        .name = L"Evening medication",
        .dose = L"10 mg",
        .interval = 24h,
    };
    settings = {
        .window_x = -200,
        .window_y = 150,
        .position_locked = true,
        .always_on_top = true,
    };
    save_medications(json_path, std::vector{medication, second}, settings);

    WidgetSettings loaded_settings;
    const std::vector<Medication> loaded = load_medications(json_path, &loaded_settings);
    CHECK(loaded.size() == 2);
    CHECK(loaded[0].id == medication.id);
    CHECK(loaded[0].name == medication.name);
    CHECK(loaded[0].dose == medication.dose);
    CHECK(loaded[0].icon_path == medication.icon_path);
    CHECK(loaded[0].interval == medication.interval);
    CHECK(loaded[0].last_taken_at == medication.last_taken_at);
    CHECK(loaded[0].enabled == medication.enabled);
    CHECK(loaded[1].id == second.id);
    CHECK(!loaded[1].icon_path);
    CHECK(!loaded[1].last_taken_at);
    CHECK(loaded_settings.window_x == settings.window_x);
    CHECK(loaded_settings.window_y == settings.window_y);
    CHECK(loaded_settings.position_locked);
    CHECK(loaded_settings.always_on_top);

    std::ifstream json(json_path);
    const std::string text{std::istreambuf_iterator<char>{json}, std::istreambuf_iterator<char>{}};
    json.close();
    CHECK(text.find("2026-08-13T10:30:00Z") != std::string::npos);
    CHECK(text.find("remaining") == std::string::npos);

    medication.dose = L"50 mg";
    save_medications(json_path, std::vector{medication, second}, settings);
    CHECK(load_medications(json_path)[0].dose == L"50 mg");
    CHECK(!std::filesystem::exists(json_path.wstring() + L".tmp"));

    std::ofstream legacy(json_path, std::ios::trunc);
    legacy << "{\"medications\": []}\n";
    legacy.close();
    loaded_settings = settings;
    CHECK(load_medications(json_path, &loaded_settings).empty());
    CHECK(!loaded_settings.window_x);
    CHECK(!loaded_settings.window_y);
    CHECK(!loaded_settings.position_locked);
    CHECK(!loaded_settings.always_on_top);
    std::filesystem::remove(json_path);
}
