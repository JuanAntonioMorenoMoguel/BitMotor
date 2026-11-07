// 09_Audio: sound effects, music and the volumes.
// - 1..4 (or the buttons): sound effects made by functions, and one from a file (Assets/chirp.ogg).
//   Pressed many times, each keeps its own limit of copies at once.
// - Song: Assets/song.bitmusic, a BitMusic song played by BitMotor's synthesizer.
//   Procedural: music computed sample by sample; "Danger" changes it as it plays.
//   Changing music crossfades. The circle bounces on every beat (Music::Pulse).
// - The volumes are the player's Settings; "Pause" muffles the music, as a pause menu would.
// Without an audio device everything goes on, in silence. Esc quits.
#include <BitMotor/BitMotor.h>

#include <cmath>

using namespace BitMotor;

namespace {

// A jump: a square wave sliding up, louder at the start.
Sound MakeJump() {
    return Sound::Synth(0.18f, [](Voice& v, float u, float dt) {
        return Synth::Square(v.Advance(0, 300 + 600 * u, dt), 0.25f) * Synth::Fall(u, 2) * 0.25f;
    });
}

// A coin: two notes, the second a fifth up.
Sound MakeCoin() {
    return Sound::Synth(0.25f, [](Voice& v, float u, float dt) {
        const float hz = u < 0.3f ? Synth::NoteToHz(83) : Synth::NoteToHz(90);
        return Synth::Square(v.Advance(0, hz, dt)) * Synth::Fall(u) * 0.2f;
    });
}

// An explosion: noise getting darker as it fades, computed once (Bake) since it is long.
Sound MakeBoom() {
    return Sound::Synth(0.8f,
                        [](Voice& v, float u, float) {
                            const float noise = v.Noise();
                            return v.Filter[0](noise, 0.4f * (1 - u) + 0.02f) * Synth::Fall(u, 1.5f) * 0.8f;
                        })
        .Bake();
}

// Arpeggios over a bass. Params[0] is the danger (0..1): faster notes, a brighter filter, hi-hats.
float Arpeggio(MusicState& m, float dt) {
    static constexpr float Chord[4] = {0, 3, 7, 10};
    const float danger = m.Params[0];
    const double beat = m.Beat(110);
    const double steps = beat * (danger > 0.5f ? 8 : 4);
    const float within = float(steps - std::floor(steps));
    const int step = int(steps) % 4;
    const float note = 57 + Chord[step] + (int(beat / 4) % 2 ? 5.0f : 0.0f);
    float lead = Synth::Square(Synth::Advance(m.Memory[0], Synth::NoteToHz(note), dt), 0.25f) * std::exp(-within * 3);
    // A one-pole low-pass kept in Memory[2]: the danger opens it.
    m.Memory[2] += (lead - m.Memory[2]) * (0.05f + 0.6f * danger);
    const float bass = Synth::Triangle(Synth::Advance(m.Memory[1], Synth::NoteToHz(33), dt));
    const double halves = beat * 2;
    const float hat = m.Noise() * std::exp(-float(halves - std::floor(halves)) * 40) * danger;
    return m.Memory[2] * 0.25f + bass * 0.25f + hat * 0.12f;
}

} // namespace

struct AudioExample : Game {
    Sound jump, coin, boom, chirp, click;
    std::optional<Song> song;
    float danger = 0;
    bool paused = false;
    const char* playing = "nothing";

    void Start() override {
        jump = MakeJump();
        coin = MakeCoin();
        boom = MakeBoom();
        chirp = Sound::Load("09_Audio/Assets/chirp.ogg", 3);
        click = Sound::Synth(0.03f, [](Voice& v, float u, float dt) {
            return Synth::Sine(v.Advance(0, 1200, dt)) * Synth::Fall(u) * 0.15f;
        });
        UI::OnClick([this] { click.Play(); }); // every button and toggle of the UI
        song = Song::Load("09_Audio/Assets/song.bitmusic");
        if (song) {
            Log::Info("song.bitmusic: {} patterns, {:.1f} s per pass", song->Patterns.size(), song->Duration());
            PlaySong();
        }
        if (!Audio::Available()) Log::Warn("No audio device: this example plays in silence");
    }

    void PlaySong() {
        if (!song) return;
        Music::Play(*song, 1.0f);
        playing = "song.bitmusic";
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        if (Input::KeyPressed(Key::Num1)) jump.Play();
        if (Input::KeyPressed(Key::Num2)) coin.Play(1, 1, 0.5f);
        if (Input::KeyPressed(Key::Num3)) boom.Play();
        if (Input::KeyPressed(Key::Num4)) chirp.Play(1, 0.9f + 0.2f * float(Time::Frame() % 3) / 2);
        Music::SetParam(0, danger);
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x10121C));
        Settings& settings = Settings::Get();
        const float h = 13;

        UI::Panel({8, 8, 148, 92}, "Sounds");
        if (UI::Button({16, 20, 64, h}, "1 Jump")) jump.Play();
        if (UI::Button({84, 20, 64, h}, "2 Coin")) coin.Play(1, 1, 0.5f);
        if (UI::Button({16, 36, 64, h}, "3 Boom")) boom.Play();
        if (UI::Button({84, 36, 64, h}, "4 Chirp", bool(chirp))) chirp.Play();
        UI::Tooltip("chirp.ogg, a file");
        bool changed = UI::Slider({16, 54, 132, h}, "Master", settings.MasterVolume);
        changed |= UI::Slider({16, 68, 132, h}, "Music", settings.MusicVolume);
        changed |= UI::Slider({16, 82, 132, h}, "Sounds", settings.SoundVolume);
        if (changed) Settings::Apply();

        UI::Panel({164, 8, 148, 92}, "Music");
        if (UI::Button({172, 20, 64, h}, "Song", bool(song))) PlaySong();
        if (UI::Button({240, 20, 64, h}, "Procedural")) {
            Music::Procedural(Arpeggio, 1.0f, 110);
            playing = "procedural";
        }
        if (UI::Button({172, 36, 64, h}, "Stop")) {
            Music::Stop(1.0f);
            playing = "nothing";
        }
        if (UI::Toggle({240, 36, 64, h}, "Pause", paused)) Audio::SetPaused(paused);
        UI::Slider({172, 54, 132, h}, "Danger", danger);
        UI::Tooltip("Music::SetParam(0): the procedural music reads it");
        Text::Draw("Playing: " + std::string(playing), {172, 72});
        Text::Draw("Beat " + Text::Number(std::floor(Music::Beat())), {172, 84});

        // Something bouncing with the music.
        const float pulse = Music::Pulse();
        UI::Circle({160, 140}, 10 + 10 * pulse, Colors::Amber.Lerp(Colors::White, pulse));
        Text::Draw(Audio::Available() ? "1-4: sounds   Esc: quit" : "No audio device: playing in silence", {160, 166},
                   {.Color = Colors::Gray, .Align = Align::Center});
    }
};

int main() {
    return Run<AudioExample>({
        .Title = "BitMotor 09 - Audio",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
