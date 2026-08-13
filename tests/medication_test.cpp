#include "medication.h"

#include <cassert>

int main() {
    using namespace std::chrono_literals;

    Medication medication{
        .id = L"morning-medication",
        .name = L"Example medication",
        .dose = L"40 mg",
        .interval = 12h,
    };
    const auto now = std::chrono::system_clock::time_point{24h};

    assert(medication.is_ready_at(now));
    assert(!medication.next_available_at());

    medication.mark_taken(now);
    assert(medication.next_available_at() == now + 12h);
    assert(medication.remaining_at(now + 11h) == 60min);
    assert(medication.is_ready_at(now + 12h));

    medication.enabled = false;
    assert(!medication.is_ready_at(now + 12h));
}
