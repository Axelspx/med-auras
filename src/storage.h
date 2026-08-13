#pragma once

#include "medication.h"

#include <filesystem>
#include <span>
#include <vector>

[[nodiscard]] std::vector<Medication> load_medications(const std::filesystem::path& path);
void save_medications(const std::filesystem::path& path, std::span<const Medication> medications);
