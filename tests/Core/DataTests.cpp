#include "Printing.h"
#include "Testing.h"

#include "Core/FilesInternal.h"

#include <BitMotor/BitMotor.h>

#include <filesystem>
#include <map>
#include <optional>
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
