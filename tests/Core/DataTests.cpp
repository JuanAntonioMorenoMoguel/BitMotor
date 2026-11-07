#include "Printing.h"
#include "Testing.h"

#include "Core/FilesInternal.h"

#include <BitMotor/BitMotor.h>

#include <filesystem>
#include <map>
#include <array>
#include <optional>
#include <set>
#include <vector>

using namespace BitMotor;

namespace DataTestTypes {

enum class Difficulty { Easy, Normal, Hard };

struct Stats {
    int Health = 10;
    float Speed = 1.5f;
};
BITMOTOR_FIELDS(Stats, Health, Speed)

struct Progress {
    int Level = 1;
    Difficulty Mode = Difficulty::Normal;
    std::string Name = "player";
    std::vector<std::string> Unlocked;
    Vec2 Checkpoint;
    Color Tint = Colors::White;
    std::optional<float> BestTime;
    std::map<std::string, int> Kills;
    Stats Hero;
    std::vector<Stats> Allies;
    bool Finished = false;
    std::uint8_t Lives = 3;
};
BITMOTOR_FIELDS(Progress, Level, Mode, Name, Unlocked, Checkpoint, Tint, BestTime, Kills, Hero, Allies, Finished, Lives)

struct Fixed {
    std::array<int, 3> Slots{};
    std::array<bool, 2> Flags{};
    std::set<std::string> Seen;
};
BITMOTOR_FIELDS(Fixed, Slots, Flags, Seen)

// As many fields as BITMOTOR_FIELDS takes.
struct Wide {
    int F1 = 1, F2 = 2, F3 = 3, F4 = 4, F5 = 5, F6 = 6, F7 = 7, F8 = 8, F9 = 9, F10 = 10, F11 = 11, F12 = 12, F13 = 13,
        F14 = 14, F15 = 15, F16 = 16, F17 = 17, F18 = 18, F19 = 19, F20 = 20, F21 = 21, F22 = 22, F23 = 23, F24 = 24,
        F25 = 25, F26 = 26, F27 = 27, F28 = 28, F29 = 29, F30 = 30, F31 = 31, F32 = 32, F33 = 33, F34 = 34, F35 = 35,
        F36 = 36, F37 = 37, F38 = 38, F39 = 39, F40 = 40, F41 = 41, F42 = 42, F43 = 43, F44 = 44, F45 = 45, F46 = 46,
        F47 = 47, F48 = 48, F49 = 49, F50 = 50, F51 = 51, F52 = 52, F53 = 53, F54 = 54, F55 = 55, F56 = 56, F57 = 57,
        F58 = 58, F59 = 59, F60 = 60, F61 = 61, F62 = 62, F63 = 63, F64 = 64, F65 = 65, F66 = 66, F67 = 67, F68 = 68,
        F69 = 69, F70 = 70, F71 = 71, F72 = 72, F73 = 73, F74 = 74, F75 = 75, F76 = 76, F77 = 77, F78 = 78, F79 = 79,
        F80 = 80, F81 = 81, F82 = 82, F83 = 83, F84 = 84, F85 = 85, F86 = 86, F87 = 87, F88 = 88, F89 = 89, F90 = 90,
        F91 = 91, F92 = 92, F93 = 93, F94 = 94, F95 = 95, F96 = 96;
};
BITMOTOR_FIELDS(Wide, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22,
                F23, F24, F25, F26, F27, F28, F29, F30, F31, F32, F33, F34, F35, F36, F37, F38, F39, F40, F41, F42, F43,
                F44, F45, F46, F47, F48, F49, F50, F51, F52, F53, F54, F55, F56, F57, F58, F59, F60, F61, F62, F63, F64,
                F65, F66, F67, F68, F69, F70, F71, F72, F73, F74, F75, F76, F77, F78, F79, F80, F81, F82, F83, F84, F85,
                F86, F87, F88, F89, F90, F91, F92, F93, F94, F95, F96)

} // namespace DataTestTypes

using namespace DataTestTypes;

TEST(DataReadsFieldsWithFallbacks) {
    Document doc = ParseDocument("heroes.json", R"({
  "goblin": {"Health": 7, "Name": "Gob", "Position": [1, 2], "Tint": "#FF0000", "Tags": ["small", "green"]},
  "orc": {"Health": 20}
})");
    CHECK(doc.Ok());
    CHECK_EQ(doc.Name(), std::string("heroes.json"));
    CHECK(doc.Keys() == std::vector<std::string>({"goblin", "orc"})); // file order
    Document goblin = doc.Child("goblin");
    CHECK(goblin.Has("Health"));
    CHECK(!goblin.Has("Armor"));
    CHECK_EQ(goblin.Get("Health", 10), 7);
    CHECK_NEAR(goblin.Get("Health", 1.0f), 7.0, 1e-6); // integers read as floats too
    CHECK_EQ(goblin.Get("Armor", 3), 3);
    CHECK_EQ(goblin.Get("Name", "?"), std::string("Gob"));
    CHECK_EQ(goblin.Get("Position", Vec2{}), Vec2(1, 2));
    CHECK_EQ(goblin.Get("Tint", Colors::White), Colors::Red);
    CHECK(goblin.Get<std::vector<std::string>>("Tags") == std::vector<std::string>({"small", "green"}));
    Document missing = doc.Child("dragon");
    CHECK(!missing.Has("Health"));
    CHECK_EQ(missing.Get("Health", 99), 99);
    CHECK_EQ(doc.Child("orc").Get("Health", 0), 20);
    CHECK(doc.Ok());
}

TEST(DataWrongTypesAreErrorsWithLine) {
    std::size_t before = Log::Problems();
    Document doc =
        ParseDocument("items.json", "{\n  \"sword\": {\n    \"Damage\": \"lots\",\n    \"Weight\": 3\n  }\n}");
    Document sword = doc.Child("sword");
    CHECK_EQ(sword.Get("Damage", 5), 5);
    CHECK(!doc.Ok());
    CHECK_EQ(Log::Problems(), before + 1);
    CHECK_EQ(Log::Recent(0), std::string("[error] items.json:3: 'sword.Damage' should be a whole number"));
    CHECK_EQ(sword.Get("Weight", 0), 3);
    CHECK_EQ(sword.Get("Weight", std::int8_t(0)), std::int8_t(3));
    Document small = ParseDocument("small.json", R"({"Value": 300})");
    CHECK_EQ(small.Get("Value", std::uint8_t(1)), std::uint8_t(1)); // out of range for a byte
    CHECK(!small.Ok());
}

TEST(DataReportsUnknownFields) {
    std::size_t before = Log::Problems();
    {
        Document doc = ParseDocument("enemy.json", "{\n  \"Health\": 3,\n  \"Speeed\": 2\n}");
        CHECK_EQ(doc.Get("Health", 0), 3);
        CHECK_NEAR(doc.Get("Speed", 1.0f), 1.0, 1e-6);
    } // reported here
    CHECK_EQ(Log::Problems(), before + 1);
    CHECK_EQ(Log::Recent(0), std::string("[warn] enemy.json:3: unknown field 'Speeed' (typo?)"));
    {
        Document untouched = ParseDocument("x.json", R"({"A": 1})"); // nobody read it: no report
        Document listed = ParseDocument("y.json", R"({"A": 1, "B": 2})");
        listed.Keys(); // listing the keys counts as reading them
    }
    CHECK_EQ(Log::Problems(), before + 1);
}

TEST(DataBrokenJsonGivesALine) {
    std::size_t before = Log::Problems();
    Document doc = ParseDocument("broken.json", "{\n  \"A\": 1,\n  \"B\": \n}");
    CHECK(!doc.Ok());
    CHECK_EQ(doc.Get("A", 5), 5);
    CHECK_EQ(Log::Problems(), before + 1);
    CHECK(Log::Recent(0).starts_with("[error] broken.json:4: broken JSON"));
}

TEST(DataStructsRoundTrip) {
    Progress saved;
    saved.Level = 7;
    saved.Mode = Difficulty::Hard;
    saved.Name = "Ñenjis \"quoted\"";
    saved.Unlocked = {"axe", "bow"};
    saved.Checkpoint = {120.5f, -3};
    saved.Tint = Color::Hex(0x336699);
    saved.BestTime = 41.1f;
    saved.Kills = {{"goblin", 12}, {"orc", 3}};
    saved.Hero = {25, 0.1f};
    saved.Allies = {{1, 2}, {3, 4}};
    saved.Finished = true;
    saved.Lives = 9;

    std::string json = Data::ToJson(saved);
    CHECK(json.find("\"Speed\": 0.1") != std::string::npos); // shortest float text
    CHECK(json.find("\"Checkpoint\": [120.5, -3.0]") != std::string::npos);
    CHECK(json.find("\"Tint\": \"#336699FF\"") != std::string::npos);

    auto loaded = Data::FromJson<Progress>(json);
    CHECK(loaded.has_value());
    CHECK_EQ(loaded->Level, 7);
    CHECK(loaded->Mode == Difficulty::Hard);
    CHECK_EQ(loaded->Name, saved.Name);
    CHECK(loaded->Unlocked == saved.Unlocked);
    CHECK_EQ(loaded->Checkpoint, saved.Checkpoint);
    CHECK_EQ(loaded->Tint, saved.Tint);
    CHECK(loaded->BestTime == saved.BestTime);
    CHECK(loaded->Kills == saved.Kills);
    CHECK_EQ(loaded->Hero.Health, 25);
    CHECK_EQ(loaded->Hero.Speed, 0.1f);
    CHECK_EQ(loaded->Allies.size(), std::size_t(2));
    CHECK_EQ(loaded->Allies[1].Speed, 4.0f);
    CHECK(loaded->Finished);
    CHECK_EQ(int(loaded->Lives), 9);
    CHECK_EQ(Data::ToJson(*loaded), json); // stable output
}

TEST(DataArraysAndSetsRoundTrip) {
    Fixed saved;
    saved.Slots = {4, 5, 6};
    saved.Flags = {true, false};
    saved.Seen = {"reveal", "intro"};
    std::string json = Data::ToJson(saved);
    CHECK(json.find("\"Seen\": [\"intro\", \"reveal\"]") != std::string::npos); // sets in order
    auto loaded = Data::FromJson<Fixed>(json);
    CHECK(loaded.has_value());
    CHECK(loaded->Slots == saved.Slots);
    CHECK(loaded->Flags == saved.Flags);
    CHECK(loaded->Seen == saved.Seen);
    auto shorter = Data::FromJson<Fixed>(R"({"Slots": [9], "Flags": [true, true, true]})");
    CHECK(shorter.has_value());
    CHECK(shorter->Slots == (std::array<int, 3>{9, 0, 0})); // missing ones keep their value, extra ones go
    CHECK(shorter->Flags == (std::array<bool, 2>{true, true}));
}

TEST(DataTakesNinetySixFields) {
    Wide wide;
    wide.F96 = 960;
    wide.F1 = -1;
    auto loaded = Data::FromJson<Wide>(Data::ToJson(wide));
    CHECK(loaded.has_value());
    CHECK_EQ(loaded->F96, 960);
    CHECK_EQ(loaded->F1, -1);
    CHECK_EQ(loaded->F48, 48);
}

TEST(DataMissingFieldsKeepDefaults) {
    auto old = Data::FromJson<Progress>(R"({"Level": 4, "BestTime": null})");
    CHECK(old.has_value());
    CHECK_EQ(old->Level, 4);
    CHECK_EQ(old->Name, std::string("player"));
    CHECK(!old->BestTime.has_value());
    CHECK_EQ(old->Hero.Speed, 1.5f);
    CHECK(!Data::FromJson<Progress>("[1, 2]").has_value()); // the root must be an object
}

TEST(DataSaveAndLoadFiles) {
    std::filesystem::path folder = std::filesystem::temp_directory_path() / "BitMotorTests" / "DataSaves";
    std::filesystem::remove_all(folder);
    Detail::SetSavePath(folder);

    CHECK(!Data::Load<Progress>("progress.json").has_value()); // first run: no file, no error
    Progress progress;
    progress.Level = 12;
    CHECK(Data::Save("progress.json", progress));
    CHECK(Files::Exists(folder / "progress.json"));
    CHECK_EQ(Data::Load<Progress>("progress.json").value_or(Progress{}).Level, 12);

    std::size_t before = Log::Problems();
    Document missing = Data::Open(folder / "nope.json");
    CHECK(!missing.Ok());
    CHECK_EQ(Log::Problems(), before + 1);
}
