#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

// Image files in memory, decoded and encoded with stb. Pixels are straight-alpha RGBA, rows from the top.
namespace BitMotor::Detail {

struct Image {
    int Width = 0, Height = 0;
    std::vector<std::uint8_t> Pixels; // Width * Height * 4

    explicit operator bool() const { return Width > 0 && Height > 0; }
};

// PNG, JPG, BMP, TGA... An empty image and the reason in `error` if the bytes are not one.
Image DecodeImage(std::span<const std::uint8_t> bytes, std::string* error = nullptr);
// Reads and decodes a file (relative paths: Files::AssetPath). Logs an error and returns empty if it fails.
Image LoadImage(const std::filesystem::path& path);

std::vector<std::uint8_t> EncodePng(const Image& image);
std::vector<std::uint8_t> EncodeBmp(const Image& image);
// Encodes by the extension (.bmp, otherwise PNG) and writes it atomically. false (and a log error) if it fails.
bool SaveImage(const Image& image, const std::filesystem::path& path);

} // namespace BitMotor::Detail
