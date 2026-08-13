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
    CHECK(load_medications(json_path).empty());

    medication.icon_path = L"icons\\morning.png";
    medication.name = L"Café \"morning\" medication";
    medication.last_taken_at = std::chrono::sys_days{std::chrono::year{2026} / 8 / 13} + 10h + 30min;
    Medication second{
        .id = L"evening-medication",
        .name = L"Evening medication",
        .dose = L"10 mg",
        .interval = 24h,
    };
    save_medications(json_path, std::vector{medication, second});

    const std::vector<Medication> loaded = load_medications(json_path);
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

    std::ifstream json(json_path);
    const std::string text{std::istreambuf_iterator<char>{json}, std::istreambuf_iterator<char>{}};
    json.close();
    CHECK(text.find("2026-08-13T10:30:00Z") != std::string::npos);
    CHECK(text.find("remaining") == std::string::npos);

    medication.dose = L"50 mg";
    save_medications(json_path, std::vector{medication, second});
    CHECK(load_medications(json_path)[0].dose == L"50 mg");
    CHECK(!std::filesystem::exists(json_path.wstring() + L".tmp"));
    std::filesystem::remove(json_path);
}
