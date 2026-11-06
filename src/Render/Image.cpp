#include "Render/ImageInternal.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <climits>

// stb is third-party code: its own warnings are not BitMotor's.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace BitMotor::Detail {

namespace {

void Append(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(context);
    auto* bytes = static_cast<const std::uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

bool Valid(const Image& image) {
    return image && image.Pixels.size() == std::size_t(image.Width) * std::size_t(image.Height) * 4;
}

} // namespace

Image DecodeImage(std::span<const std::uint8_t> bytes, std::string* error) {
    Image image;
    if (bytes.empty() || bytes.size() > std::size_t(INT_MAX)) {
        if (error) *error = bytes.empty() ? "empty file" : "file too big";
        return image;
    }
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &width, &height, &channels, 4);
    if (!pixels) {
        if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "unknown image format";
        return image;
    }
    image.Width = width;
    image.Height = height;
    image.Pixels.assign(pixels, pixels + std::size_t(width) * std::size_t(height) * 4);
    stbi_image_free(pixels);
    return image;
}

Image LoadImage(const std::filesystem::path& path) {
    std::filesystem::path full = path.is_relative() ? Files::AssetPath(path) : path;
    auto bytes = Files::ReadBytes(full);
    if (!bytes) {
        Log::Error("Cannot read the image {}", full.string());
        return {};
    }
    std::string error;
    Image image = DecodeImage(*bytes, &error);
    if (!image) Log::Error("Cannot decode the image {}: {}", full.string(), error);
    return image;
}

std::vector<std::uint8_t> EncodePng(const Image& image) {
    std::vector<std::uint8_t> out;
    if (Valid(image))
        stbi_write_png_to_func(Append, &out, image.Width, image.Height, 4, image.Pixels.data(), image.Width * 4);
    return out;
}

std::vector<std::uint8_t> EncodeBmp(const Image& image) {
    std::vector<std::uint8_t> out;
    if (Valid(image)) stbi_write_bmp_to_func(Append, &out, image.Width, image.Height, 4, image.Pixels.data());
    return out;
}

bool SaveImage(const Image& image, const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    std::vector<std::uint8_t> bytes = ext == ".bmp" ? EncodeBmp(image) : EncodePng(image);
    if (bytes.empty()) {
        Log::Error("Cannot encode the image {}", path.string());
        return false;
    }
    return Files::WriteBytes(path, bytes);
}

} // namespace BitMotor::Detail
