#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Render/Texture.h"
#include "Render/ImageInternal.h"

#include <filesystem>

using namespace BitMotor;

namespace {

// 2x2: red, green / blue, half-transparent white.
Detail::Image Sample() {
    Detail::Image image;
    image.Width = 2;
    image.Height = 2;
    image.Pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128};
    return image;
}

std::filesystem::path TempFile(const char* name) {
    return std::filesystem::temp_directory_path() / "BitMotorTests" / name;
}

} // namespace

TEST(ImagePngRoundTrip) {
    Detail::Image image = Sample();
    std::vector<std::uint8_t> png = Detail::EncodePng(image);
    CHECK(png.size() > 8);
    CHECK_EQ(int(png[1]), int('P')); // \x89PNG
    Detail::Image back = Detail::DecodeImage(png);
    CHECK_EQ(back.Width, 2);
    CHECK_EQ(back.Height, 2);
    CHECK(back.Pixels == image.Pixels);
}

TEST(ImageBmpRoundTrip) {
    std::vector<std::uint8_t> bmp = Detail::EncodeBmp(Sample());
    CHECK_EQ(int(bmp[0]), int('B'));
    Detail::Image back = Detail::DecodeImage(bmp);
    CHECK_EQ(back.Width, 2);
    CHECK(back.Pixels == Sample().Pixels);
}

TEST(ImageRejectsGarbage) {
    std::vector<std::uint8_t> junk = {1, 2, 3, 4, 5};
    std::string error;
    Detail::Image image = Detail::DecodeImage(junk, &error);
    CHECK(!image);
    CHECK(!error.empty());
    CHECK(!Detail::DecodeImage({}, &error));
    CHECK(Detail::EncodePng(Detail::Image{}).empty());
}

TEST(TextureLoadsFromAFile) {
    std::filesystem::path png = TempFile("Texture.png");
    CHECK(Detail::SaveImage(Sample(), png));
    Texture texture = Texture::Load(png, Filter::Linear);
    CHECK(bool(texture));
    CHECK_EQ(texture.Width(), 2);
    CHECK_EQ(texture.Size(), Vec2(2, 2));
    CHECK(texture.GetFilter() == Filter::Linear);
    // Copies share the image.
    Texture copy = texture;
    CHECK(copy == texture);
    std::filesystem::remove(png);
}

TEST(TextureFailuresStayEmpty) {
    CHECK(!Texture());
    CHECK(!Texture::Load(TempFile("Missing.png")));
    std::uint8_t few[3] = {};
    CHECK(!Texture::FromPixels(2, 2, few));
    CHECK_EQ(Texture().Width(), 0);
    std::uint8_t one[4] = {9, 9, 9, 9};
    CHECK_EQ(Texture::FromPixels(1, 1, one).Height(), 1);
}
