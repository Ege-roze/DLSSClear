#pragma once
#include <filesystem>
int RunValidation(const std::filesystem::path& shader, const std::filesystem::path& destination, bool warp,
    const std::filesystem::path& referenceShader = {});

int RunLocalizationValidation(const std::filesystem::path& destination);
