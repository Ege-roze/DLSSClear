#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

struct Image {
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> pixels; // Straight RGBA, SDR sRGB code values.
};
Image LoadImageFile(const std::filesystem::path& path);
void SavePng(const std::filesystem::path& path, const Image& image);
Image MakeTestImage(unsigned width = 1280, unsigned height = 720);
