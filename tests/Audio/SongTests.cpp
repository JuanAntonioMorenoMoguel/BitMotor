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

namespace {

// The same song as Simple, built in code.
Song SimpleSong() {
    Song song;
    song.Loop = -1;
    Instrument lead;
    lead.Attack = lead.Decay = lead.Release = 0;
    lead.Sustain = 1;
    song.Instruments["lead"] = lead;
    song.Patterns["a"] = {4, {{"lead", {"A4", "", "off", "C4:0.5"}}}};
    song.Order = {"a", "a"};
    return song;
}

} // namespace

TEST(NoteEntriesAreReadAndWritten) {
    std::optional<NoteEntry> c = NoteEntry::Parse(" C4:0.5 ");
    CHECK(c.has_value());
    if (c) {
        CHECK(c->Type == NoteEntry::Kind::On);
        CHECK_EQ(c->Note, 60.0f);
        CHECK_EQ(c->Volume, 0.5f);
        CHECK_EQ(c->ToString(), std::string("C4:0.5"));
    }
    CHECK(NoteEntry::Parse("off")->Type == NoteEntry::Kind::Off);
    CHECK(NoteEntry::Parse("")->Type == NoteEntry::Kind::Keep);
    CHECK(NoteEntry::Parse("---")->Type == NoteEntry::Kind::Keep);
    CHECK(!NoteEntry::Parse("X9"));
    CHECK(!NoteEntry::Parse("C4:-1"));
    CHECK_EQ(NoteEntry::Parse("Bb2")->ToString(), std::string("A#2"));
    CHECK_EQ(NoteEntry{}.ToString(), std::string(""));
    CHECK_EQ((NoteEntry{NoteEntry::Kind::Off}).ToString(), std::string("off"));
    CHECK_EQ(NoteName(60), std::string("C4"));
    CHECK_EQ(NoteName(61), std::string("C#4"));
    CHECK_EQ(NoteName(0), std::string("C-1"));
    CHECK_EQ(NoteName(127), std::string("G9"));
    CHECK_EQ(NoteName(61.5f), std::string("61.5"));
    CHECK_EQ(ParseNote(NoteName(69)).value_or(0), 69.0f);
}

TEST(SongsKeepTheirBeats) {
    Song song = SimpleSong();
    CHECK_EQ(song.Beats, 4);
    song.Beats = 3;
    fs::path file = fs::temp_directory_path() / "BitMotorTests" / "Songs" / "beats.bitmusic";
    CHECK(song.Save(file));
    std::optional<Song> again = Song::Load(file);
    CHECK(again.has_value());
    if (again) CHECK_EQ(again->Beats, 3);
    // It does not change the sound.
    Song four = SimpleSong();
    CHECK(song.Render() == four.Render());
}

TEST(SongsRenderWhole) {
    Song song = SimpleSong();
    std::vector<float> once = song.Render();
    // 8 rows of 6000 samples; the last note is released when its row is over (Release 0: at once).
    CHECK_EQ(once.size(), std::size_t(2 * (8 * Row + 1)));
    CHECK_NEAR(Peak(once, 7 * Row + 10, 8 * Row), 0.25f, 1e-5f);
    // The same samples as playing it.
    std::vector<float> played = Detail::RenderSong(song, 1.0);
    CHECK(std::equal(played.begin(), played.end(), once.begin()));
    // Repeats: again from Loop.
    song.Loop = 1;
    CHECK_EQ(song.Render(1).size(), std::size_t(2 * (8 * Row + 1)));
    CHECK_EQ(song.Render(3).size(), std::size_t(2 * (16 * Row + 1)));
    song.Loop = -1;
    CHECK_EQ(song.Render(3).size(), once.size());
    // The same input, the same samples.
    CHECK(song.Render() == once);
}

TEST(SongsStartAnywhere) {
    Song song = SimpleSong();
    song.Order = {"a", "missing", "a"};
    std::vector<float> block(2 * (Row + 10));
    // From the second "a" (position 2 in the file's Order), row 3: C4 at half volume.
    auto source = Detail::MakeSongSource(song, false, 2, 3);
    CHECK_EQ(source->CurrentPosition().Order, 2);
    CHECK_EQ(source->CurrentPosition().Row, 3);
    CHECK(source->Render(block.data(), Row));
    CHECK_NEAR(Peak(block, 10, Row), 0.25f, 1e-5f);
    CHECK_EQ(source->CurrentPosition().Order, 2);
    CHECK_EQ(source->CurrentPosition().Row, 3);
    // Position 1 does not exist: it starts at the next one that does.
    CHECK_EQ(Detail::MakeSongSource(song, false, 1)->CurrentPosition().Order, 2);
    // Out of range: the beginning.
    auto start = Detail::MakeSongSource(song, false, 9, 99);
    CHECK_EQ(start->CurrentPosition().Order, 0);
    CHECK_EQ(start->CurrentPosition().Row, 0);
    // Rows go by.
    std::fill(block.begin(), block.end(), 0.0f);
    start->Render(block.data(), Row + 10);
    CHECK_EQ(start->CurrentPosition().Row, 1);
}

TEST(SongChannelsMuteAndMeasure) {
    Detail::ResetAudio();
    Song song = SimpleSong();
    song.Loop = 0;
    Music::Play(song, 0);
    CHECK_EQ(Music::Position().Order, 0);
    std::vector<float> out(2 * 1024);
    Music::Mute(0);
    CHECK(Music::Muted(0));
    Detail::MixAudio(out.data(), 1024);
    CHECK_EQ(Peak(out, 0, 1024), 0.0f);
    CHECK_EQ(Music::Level(0), 0.0f);
    Music::Mute(0, false);
    CHECK(!Music::Muted(0));
    std::fill(out.begin(), out.end(), 0.0f);
    Detail::MixAudio(out.data(), 1024);
    CHECK_NEAR(Peak(out, 0, 1024), 0.5f, 1e-3f);
    CHECK_NEAR(Music::Level(0), 0.5f, 1e-3f);
    CHECK_EQ(Music::Level(5), 0.0f);
    Music::Mute(99); // out of range: ignored
    CHECK(!Music::Muted(99));
    Music::Stop(0);
    CHECK_EQ(Music::Position().Order, -1);
    Detail::ResetAudio();
}

TEST(SingleNotesPlayAndRelease) {
    Detail::ResetAudio();
    Instrument lead;
    lead.Attack = lead.Decay = lead.Release = 0;
    lead.Sustain = 1;
    std::vector<float> out(2 * 4800);
    const int id = Music::PlayNote(lead, 69);
    Detail::MixAudio(out.data(), 4800);
    CHECK_NEAR(Peak(out, 0, 4800), 0.5f, 1e-3f);
    CHECK(std::abs(Rises(out, 0, 4800) - 44) <= 1); // 0.1 s at 440 Hz
    Music::ReleaseNote(id);
    std::fill(out.begin(), out.end(), 0.0f);
    Detail::MixAudio(out.data(), 4800);
    CHECK_EQ(Peak(out, 2, 4800), 0.0f);
    // Two at once, then all released.
    Music::PlayNote(lead, 60, 0.5f);
    Music::PlayNote(lead, 64, 0.5f);
    std::fill(out.begin(), out.end(), 0.0f);
    Detail::MixAudio(out.data(), 4800);
    CHECK(Peak(out, 0, 4800) > 0.3f);
    Music::ReleaseNotes();
    std::fill(out.begin(), out.end(), 0.0f);
    Detail::MixAudio(out.data(), 4800);
    CHECK_EQ(Peak(out, 2, 4800), 0.0f);
    Detail::ResetAudio();
}
