#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Settings.h"
#include "BitMotor/Render/Atlas.h"
#include "Render/ImageInternal.h"
#include "Render/SpritesInternal.h"

#include <filesystem>

using namespace BitMotor;

TEST(LightFollowsTheCamera) {
    Camera2D camera;
    camera.Position = {100, 100};
    camera.Zoom = 2;
    Detail::LightInstance l = Detail::MakeLight(
        Light2D{.Position = {110, 100}, .Height = 10, .Radius = 50, .Color = Colors::Red, .Intensity = 0.5f}, camera);
    Vec2 at = camera.ToScreen({110, 100});
    CHECK_EQ(l.X, at.X);
    CHECK_EQ(l.Y, at.Y);
    CHECK_EQ(l.Radius, 100.0f);
    CHECK_EQ(l.Height, 20.0f);
    CHECK_EQ(l.R, 0.5f);
    CHECK_EQ(l.G, 0.0f);
}

TEST(LightsQueueForOneFrame) {
    Detail::ResetSprites();
    Screen::Draw(Light2D{.Radius = 30});
    Screen::Draw(Light2D{.Radius = 0});    // reaches nothing: skipped
    Screen::Draw(Light2D{.Intensity = 0}); // gives nothing: skipped
    CHECK_EQ(Detail::LightQueue().size(), std::size_t(1));
    Detail::ResetSprites();
    CHECK(Detail::LightQueue().empty());
}

TEST(LightingSettingsStay) {
    CHECK(!Lighting::Enabled()); // off until the game turns it on
    Lighting::Enable(true);
    Lighting::SetAmbient(Colors::Navy);
    Lighting::SetStepped(true);
    Lighting::SetBloom(-1); // negative: off
    CHECK(Lighting::Enabled());
    CHECK_EQ(Lighting::Ambient(), Colors::Navy);
    CHECK(Lighting::Stepped());
    CHECK_EQ(Lighting::Bloom(), 0.0f);
    Lighting::SetBloom(0.6f, 0.9f);
    CHECK_EQ(Lighting::BloomThreshold(), 0.9f);
    Screen::SetEffects({.Vignette = 0.3f, .Saturation = 1.2f});
    CHECK_EQ(Screen::Effects().Vignette, 0.3f);
    CHECK_EQ(Screen::Effects().Grain, 0.0f);
    // Back to the defaults for the other tests.
    Lighting::Enable(false);
    Lighting::SetStepped(false);
    Lighting::SetBloom(0);
    Screen::SetEffects({});
}

TEST(FlashFadesAndRespectsTheSetting) {
    Screen::Flash(Colors::Red, 0.5f);
    CHECK_EQ(Detail::FlashAmount(), 1.0f);
    CHECK_EQ(Detail::FlashColor(), Colors::Red);
    Detail::AdvanceFlash(0.25f);
    CHECK_NEAR(Detail::FlashAmount(), 0.5f, 1e-5f);
    Detail::AdvanceFlash(1);
    CHECK_EQ(Detail::FlashAmount(), 0.0f);
    // A player who turned flashes off never sees one.
    Settings::Get().Flashes = false;
    Screen::Flash(Colors::White, 1);
    CHECK_EQ(Detail::FlashAmount(), 0.0f);
    Settings::Get().Flashes = true;
}

TEST(AtlasLoadsItsNormalMap) {
    std::filesystem::path folder = std::filesystem::temp_directory_path() / "BitMotorTests" / "AtlasNormals";
    Detail::Image image;
    image.Width = image.Height = 4;
    image.Pixels.assign(64, 200);
    CHECK(Detail::SaveImage(image, folder / "sheet.png"));
    CHECK(Files::Write(folder / "sheet.json", R"({"sprites": {"dot": [0, 0, 4, 4, 2, 2]}})"));
    Atlas plain = Atlas::Load(folder / "sheet.json");
    CHECK(bool(plain));
    CHECK(!plain.Image().Normals()); // no sheet_n.png: flat
    CHECK(Detail::SaveImage(image, folder / "sheet_n.png"));
    Atlas lit = Atlas::Load(folder / "sheet.json");
    CHECK(bool(lit.Image().Normals()));
    CHECK_EQ(lit.Image().Normals().Width(), 4);
    std::filesystem::remove_all(folder);
}
