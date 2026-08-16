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
    // Card background. When blur is on, the colour below is a tint over the blurred backdrop; when
    // it is off, the colour is the card itself. Either way the alpha is what you see through.
    // Both need Windows Composition: without it the widget falls back to an opaque card and these
    // are ignored. The defaults reproduce the appearance the blur was tuned with (black at 0.39).
    bool background_blur{true};
    unsigned char background_red{0};
    unsigned char background_green{0};
    unsigned char background_blue{0};
    unsigned char background_alpha{99};
};

[[nodiscard]] std::vector<Medication> load_medications(
    const std::filesystem::path& path, WidgetSettings* settings = nullptr);
void save_medications(
    const std::filesystem::path& path, std::span<const Medication> medications,
    const WidgetSettings& settings = {});
