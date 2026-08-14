#pragma once

#include "medication.h"

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

struct WidgetSettings {
    std::optional<int> window_x;
    std::optional<int> window_y;
    bool position_locked{};
    bool always_on_top{};
};

[[nodiscard]] std::vector<Medication> load_medications(
    const std::filesystem::path& path, WidgetSettings* settings = nullptr);
void save_medications(
    const std::filesystem::path& path, std::span<const Medication> medications,
    const WidgetSettings& settings = {});
