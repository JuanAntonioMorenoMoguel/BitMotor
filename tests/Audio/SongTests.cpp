#include "Printing.h"
#include "Testing.h"

#include "Audio/AudioInternal.h"

#include <BitMotor/BitMotor.h>

#include <cmath>
#include <filesystem>

using namespace BitMotor;
namespace fs = std::filesystem;

namespace {

// 120 beats per minute, 4 rows per beat: a row is 0.125 s, 6000 samples.
constexpr int Row = 6000;

const char* Simple = R"({
  "Format": "bitmusic", "Version": 1, "Bpm": 120, "Rows": 4, "Volume": 1, "Loop": -1,
  "Instruments": {
    "lead": {"Wave": "square", "Volume": 0.5, "Attack": 0, "Decay": 0, "Sustain": 1, "Release": 0}
  },
  "Patterns": {
    "a": {"Length": 4, "Tracks": [{"Instrument": "lead", "Notes": ["A4", "", "off", "C4:0.5"]}]}
  },
  "Order": ["a", "a"]
})";

float Peak(const std::vector<float>& samples, std::size_t from, std::size_t to, int channel = 0) {
    float peak = 0;
    for (std::size_t i = from; i < to; ++i) peak = std::max(peak, std::abs(samples[2 * i + std::size_t(channel)]));
    return peak;
}

// Zero crossings from - to + between two frames: the frequency of a plain wave.
int Rises(const std::vector<float>& samples, std::size_t from, std::size_t to) {
    int rises = 0;
    for (std::size_t i = from + 1; i < to; ++i)
        if (samples[2 * (i - 1)] < 0 && samples[2 * i] >= 0) ++rises;
    return rises;
}

} // namespace

TEST(NotesHaveNames) {
    CHECK_EQ(Detail::ParseNote("C4").value_or(0), 60.0f);
    CHECK_EQ(Detail::ParseNote("A4").value_or(0), 69.0f);
    CHECK_EQ(Detail::ParseNote("F#3").value_or(0), 54.0f);
    CHECK_EQ(Detail::ParseNote("Bb2").value_or(0), 46.0f);
    CHECK_EQ(Detail::ParseNote("c-1").value_or(1), 0.0f);
    CHECK_EQ(Detail::ParseNote("60").value_or(0), 60.0f);
    CHECK_EQ(Detail::ParseNote("61.5").value_or(0), 61.5f);
    CHECK(!Detail::ParseNote("H4"));
    CHECK(!Detail::ParseNote("C"));
    CHECK(!Detail::ParseNote("C10"));
    CHECK(!Detail::ParseNote("200"));
    CHECK(!Detail::ParseNote(""));
}

TEST(SongsAreReadAndWritten) {
    std::optional<Song> song = Song::Parse(Simple);
    CHECK(song.has_value());
    if (!song) return;
    CHECK_EQ(song->Bpm, 120.0f);
    CHECK_EQ(song->Instruments.size(), std::size_t(1));
    CHECK_EQ(song->Instruments["lead"].Wave, std::string("square"));
    CHECK_EQ(song->Instruments["lead"].Width, 0.5f); // missing fields keep their defaults
    CHECK_EQ(song->Patterns["a"].Tracks[0].Notes[3], std::string("C4:0.5"));
    CHECK_NEAR(song->Duration(), 1.0, 1e-9); // 8 rows of 0.125 s

    fs::path file = fs::temp_directory_path() / "BitMotorTests" / "Songs" / "simple.bitmusic";
    CHECK(song->Save(file));
    std::optional<Song> again = Song::Load(file);
    CHECK(again.has_value());
    if (again) {
        CHECK_EQ(again->Order.size(), std::size_t(2));
        CHECK_EQ(again->Patterns["a"].Length, 4);
        CHECK_EQ(again->Instruments["lead"].Volume, 0.5f);
    }
    // Not a song, a broken one, or one that cannot play.
    CHECK(!Song::Parse(R"({"Format": "bitsprite"})"));
    CHECK(!Song::Parse("{"));
    CHECK(!Song::Parse(R"({"Format": "bitmusic", "Bpm": 0})"));
    CHECK(!Song::Load(file.parent_path() / "missing.bitmusic"));
    // Unknown names and notes are only warnings: playing skips them.
    std::optional<Song> odd = Song::Parse(R"({"Format": "bitmusic",
        "Patterns": {"p": {"Length": 2, "Tracks": [{"Instrument": "nobody", "Notes": ["X9"]}]}},
        "Order": ["p", "missing"]})");
    CHECK(odd.has_value());
    if (odd) CHECK_NEAR(odd->Duration(), 0.25, 1e-9);
}

TEST(SongsPlayTheirNotes) {
    std::optional<Song> song = Song::Parse(Simple);
    if (!song) return;
    std::vector<float> out = Detail::RenderSong(*song, 1.2);
    // Row 0: A4 (440 Hz) at the instrument's volume.
    CHECK_NEAR(Peak(out, 0, Row), 0.5f, 1e-5f);
    CHECK(std::abs(Rises(out, 0, 2 * Row) - 110) <= 1); // 0.25 s at 440 Hz
    // Row 1 keeps it; row 2 releases it (Release 0: silence at once).
    CHECK_NEAR(Peak(out, Row, 2 * Row), 0.5f, 1e-5f);
    CHECK_EQ(Peak(out, 2 * Row + 1, 3 * Row), 0.0f);
    // Row 3: C4 at half volume, which goes on into the next pattern's row 0, where A4 starts again.
    CHECK_NEAR(Peak(out, 3 * Row + 10, 4 * Row), 0.25f, 1e-5f);
    CHECK(std::abs(Rises(out, 3 * Row, 4 * Row) - 33) <= 1); // 0.125 s at 261.6 Hz
    CHECK_NEAR(Peak(out, 4 * Row + 10, 5 * Row), 0.5f, 1e-5f);
    // Loop -1: after the Order, the last note is released and the song is over.
    CHECK_EQ(Peak(out, 8 * Row + 1, out.size() / 2), 0.0f);
}

TEST(SongsLoopAndEnd) {
    std::optional<Song> song = Song::Parse(Simple);
    if (!song) return;
    std::vector<float> block(2 * 48000);
    auto once = Detail::MakeSongSource(*song, true); // the song's Loop is -1: it ends anyway
    CHECK(!once->Render(block.data(), 48000 + 100));
    song->Loop = 1;
    auto looping = Detail::MakeSongSource(*song, true);
    CHECK(looping->Render(block.data(), 48000));
    std::fill(block.begin(), block.end(), 0.0f);
    CHECK(looping->Render(block.data(), 48000)); // still playing pattern "a" again
    CHECK_NEAR(Peak(block, 0, Row), 0.5f, 1e-5f);
    auto unlooped = Detail::MakeSongSource(*song, false); // Music::Play(song, fade, false)
    std::vector<float> longer(2 * 60000);
    CHECK(!unlooped->Render(longer.data(), 60000));
    // An empty song ends at once.
    Song empty;
    CHECK(!Detail::MakeSongSource(empty, true)->Render(block.data(), 10));
}

TEST(InstrumentsShapeTheSound) {
    Song song = *Song::Parse(Simple);
    Instrument& lead = song.Instruments["lead"];
    // An envelope: 0.1 s up, 0.1 s down to half, then held.
    lead.Attack = 0.1f;
    lead.Decay = 0.1f;
    lead.Sustain = 0.5f;
    lead.Wave = "sine";
    std::vector<float> out = Detail::RenderSong(song, 0.25);
    CHECK(Peak(out, 0, 480) < 0.06f);                   // the first 10 ms: still rising
    CHECK(Peak(out, 4500, 5000) > 0.45f);               // the top, at 0.1 s
    CHECK_NEAR(Peak(out, 11000, 12000), 0.25f, 0.005f); // held at half
    // Panned right: nothing on the left.
    lead = Instrument{};
    lead.Pan = 1;
    out = Detail::RenderSong(song, 0.1);
    CHECK_EQ(Peak(out, 0, 4800, 0), 0.0f);
    CHECK(Peak(out, 0, 4800, 1) > 0.4f);
    // A sweep down: fewer cycles in the same time.
    lead = Instrument{};
    lead.Sustain = 1;
    lead.Wave = "square";
    std::vector<float> plain = Detail::RenderSong(song, 0.1);
    lead.Sweep = -12;
    lead.SweepTime = 0.01f;
    std::vector<float> swept = Detail::RenderSong(song, 0.1);
    CHECK(std::abs(Rises(plain, 0, 4800) - 44) <= 1);
    CHECK(std::abs(Rises(swept, 0, 4800) - 23) <= 2); // an octave down after 10 ms
    // Noise, crushed: few distinct levels.
    lead.Sweep = 0;
    lead.Wave = "noise";
    lead.Crush = 2;
    out = Detail::RenderSong(song, 0.05);
    for (std::size_t i = 100; i < 2400; ++i) {
        CHECK(std::abs(out[2 * i] * 2 - std::round(out[2 * i] * 2)) < 1e-4f); // -0.5, 0 or 0.5
    }
}
