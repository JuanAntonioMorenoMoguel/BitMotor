#include "Printing.h"
#include "Testing.h"

#include "Core/FilesInternal.h"
#include "Core/InputInternal.h"

#include <filesystem>

using namespace BitMotor;
using Detail::InputTick;

namespace {
enum class Action { Left, Right, Up, Down, Jump, Fire };

void BindAll() {
    Detail::ResetInput();
    Input::Bind(Action::Left, "left", {Key::A, Key::Left, Button::LeftStickLeft});
    Input::Bind(Action::Right, "right", {Key::D, Key::Right, Button::LeftStickRight});
    Input::Bind(Action::Up, "up", {Key::W, Button::LeftStickUp});
    Input::Bind(Action::Down, "down", {Key::S, Button::LeftStickDown});
    Input::Bind(Action::Jump, "jump", {Key::Space, Button::South});
    Input::Bind(Action::Fire, "fire", {MouseButton::Left, Button::RightTrigger});
}
} // namespace

TEST(InputPressedIsSeenByExactlyOneUpdate) {
    BindAll();
    Detail::InputKey(Key::Space, true);
    InputTick();
    CHECK(Input::Pressed(Action::Jump));
    CHECK(Input::Down(Action::Jump));
    CHECK(Input::KeyPressed(Key::Space));
    InputTick(); // still held
    CHECK(!Input::Pressed(Action::Jump));
    CHECK(Input::Down(Action::Jump));
    Detail::InputKey(Key::Space, false);
    InputTick();
    CHECK(Input::Released(Action::Jump));
    CHECK(!Input::Down(Action::Jump));
    InputTick();
    CHECK(!Input::Released(Action::Jump));
}

TEST(InputQuickTapBetweenUpdatesIsNotLost) {
    BindAll();
    Detail::InputKey(Key::Space, true);
    Detail::InputKey(Key::Space, false); // down and up before the Update
    InputTick();
    CHECK(Input::Pressed(Action::Jump));
    InputTick();
    CHECK(Input::Released(Action::Jump));
}

TEST(InputAnyControlOfAnActionWorks) {
    BindAll();
    Detail::InputPadButton(Button::South, true);
    InputTick();
    CHECK(Input::Pressed(Action::Jump));
    CHECK(Input::UsingGamepad());
    Detail::InputMouseButton(MouseButton::Left, true);
    InputTick();
    CHECK(Input::Pressed(Action::Fire));
    CHECK(Input::MousePressed(MouseButton::Left));
    CHECK(!Input::UsingGamepad());
    CHECK(!Input::Pressed(Action::Left)); // unrelated actions stay quiet
    CHECK(!Input::Down(42));              // never bound: always false
}

TEST(InputAxesMixKeysAndSticks) {
    BindAll();
    Detail::InputKey(Key::D, true);
    InputTick();
    CHECK_NEAR(Input::Axis(Action::Left, Action::Right), 1.0, 1e-6);
    Detail::InputKey(Key::W, true);
    InputTick();
    Vec2 diagonal = Input::Axis(Action::Left, Action::Right, Action::Up, Action::Down);
    CHECK_NEAR(Length(diagonal), 1.0, 1e-5); // not faster on diagonals
    CHECK(diagonal.X > 0 && diagonal.Y < 0); // up is -Y
    Detail::InputKey(Key::D, false);
    Detail::InputKey(Key::W, false);

    // Half a stick: analog strength after the dead zone (0.2).
    Detail::InputPadAxis(Detail::PadAxis::LeftX, -0.6f);
    InputTick();
    CHECK_NEAR(Input::Axis(Action::Left, Action::Right), -0.5, 1e-5);
    CHECK(Input::Down(Action::Left));                   // 0.5 counts as down
    Detail::InputPadAxis(Detail::PadAxis::LeftX, 0.1f); // inside the dead zone
    InputTick();
    CHECK_NEAR(Input::Axis(Action::Left, Action::Right), 0.0, 1e-6);
    CHECK_EQ(Input::LeftStick(), Vec2(0, 0));

    Detail::InputPadAxis(Detail::PadAxis::RightTrigger, 0.7f);
    InputTick();
    CHECK_NEAR(Input::Strength(Action::Fire), 0.7, 1e-6);
    CHECK(Input::Pressed(Action::Fire));
}

TEST(InputMouseAndForce) {
    BindAll();
    Detail::InputMouseMove({10, 20});
    InputTick();
    Detail::InputMouseMove({15, 18});
    Detail::InputMouseWheel(1);
    Detail::InputMouseWheel(2);
    InputTick();
    CHECK_EQ(Input::Mouse(), Vec2(15, 18));
    CHECK_EQ(Input::MouseDelta(), Vec2(5, -2));
    CHECK_NEAR(Input::MouseWheel(), 3.0, 1e-6);
    InputTick();
    CHECK_EQ(Input::MouseDelta(), Vec2(0, 0));

    Input::Force(Action::Jump, true);
    InputTick();
    CHECK(Input::Pressed(Action::Jump));
    Input::Force(Action::Jump, false);
    InputTick();
    CHECK(Input::Released(Action::Jump));
}

TEST(InputRebindReplacesTheSameKindOfControl) {
    BindAll();
    Input::Rebind(Action::Jump);
    CHECK(Input::IsRebinding());
    Detail::InputKey(Key::J, true);
    CHECK(!Input::IsRebinding());
    CHECK_EQ(Input::KeyName(Action::Jump), std::string("J"));
    CHECK_EQ(Input::ButtonName(Action::Jump), std::string("Pad South")); // the gamepad one stays
    CHECK_EQ(Input::Controls(Action::Jump).size(), std::size_t(2));

    Input::Rebind(Action::Jump);
    Detail::InputPadAxis(Detail::PadAxis::LeftTrigger, 0.9f); // triggers and sticks can be chosen too
    CHECK_EQ(Input::ButtonName(Action::Jump), std::string("Pad Left Trigger"));

    Input::Rebind(Action::Jump);
    Detail::InputKey(Key::Escape, true); // cancels
    CHECK(!Input::IsRebinding());
    CHECK_EQ(Input::KeyName(Action::Jump), std::string("J"));

    Input::ResetBindings();
    CHECK_EQ(Input::KeyName(Action::Jump), std::string("Space"));
    CHECK_EQ(Input::KeyName(Action::Fire), std::string("Mouse Left"));
}

TEST(InputNamesRoundTrip) {
    for (int k = 1; k < int(Key::Count); ++k) {
        Control control = Key::Unknown;
        CHECK(ControlFromName(Name(Key(k)), control));
        CHECK(control == Control(Key(k)));
    }
    for (int b = 0; b < int(Button::Count); ++b) {
        Control control = Key::Unknown;
        CHECK(ControlFromName(Name(Button(b)), control));
        CHECK(control == Control(Button(b)));
    }
    Control control = Key::Unknown;
    CHECK(ControlFromName("Mouse Right", control));
    CHECK(control == Control(MouseButton::Right));
    CHECK(!ControlFromName("Banana", control));
}

TEST(InputBindingsSaveAndLoad) {
    std::filesystem::path folder = std::filesystem::temp_directory_path() / "BitMotorTests" / "Controls";
    std::filesystem::remove_all(folder);
    Detail::SetSavePath(folder);
    BindAll();
    CHECK(!Input::LoadBindings()); // first run: no file, defaults stay
    Input::Rebind(Action::Jump);
    Detail::InputKey(Key::K, true);
    CHECK(Input::SaveBindings());

    BindAll(); // as on the next launch
    CHECK_EQ(Input::KeyName(Action::Jump), std::string("Space"));
    CHECK(Input::LoadBindings());
    CHECK_EQ(Input::KeyName(Action::Jump), std::string("K"));
    CHECK_EQ(Input::ButtonName(Action::Jump), std::string("Pad South"));

    // Hand-edited file with a typo: warned, the rest still loads.
    CHECK(Files::Write(folder / "controls.json", R"({"jump": ["Spacee", "Pad East"], "fire": ["Mouse Right"]})"));
    std::size_t before = Log::Problems();
    CHECK(Input::LoadBindings());
    CHECK_EQ(Log::Problems(), before + 1);
    CHECK_EQ(Input::KeyName(Action::Jump), std::string("-"));
    CHECK_EQ(Input::ButtonName(Action::Jump), std::string("Pad East"));
    CHECK_EQ(Input::KeyName(Action::Fire), std::string("Mouse Right"));
}

TEST(InputRecordAndReplay) {
    BindAll();
    Input::StartRecording();
    InputTick();                        // tick 0: nothing
    Detail::InputKey(Key::Space, true); // tick 1: jump pressed
    InputTick();
    InputTick();                         // tick 2: held
    Detail::InputKey(Key::Space, false); // tick 3: released
    Detail::InputPadAxis(Detail::PadAxis::LeftX, 0.6f);
    InputTick();
    InputRecording recording = Input::StopRecording();
    CHECK(!Input::IsRecording());
    CHECK_EQ(recording.Ticks, std::uint64_t(4));
    CHECK_EQ(recording.Changes.size(), std::size_t(3)); // jump on, jump off, right 0.5

    // Through JSON, as a saved demo would.
    auto loaded = Data::FromJson<InputRecording>(Data::ToJson(recording));
    CHECK(loaded.has_value());

    BindAll();
    Detail::InputKey(Key::A, true); // the devices are ignored during the replay
    Input::Replay(*loaded);
    std::vector<bool> pressed, down;
    for (int i = 0; i < 4; ++i) {
        InputTick();
        pressed.push_back(Input::Pressed(Action::Jump));
        down.push_back(Input::Down(Action::Jump));
        CHECK(!Input::Down(Action::Left));
    }
    CHECK(pressed == std::vector<bool>({false, true, false, false}));
    CHECK(down == std::vector<bool>({false, true, true, false}));
    CHECK_NEAR(Input::Axis(Action::Left, Action::Right), 0.5, 1e-5);
    CHECK(Input::IsReplaying());
    InputTick(); // past the end: back to the devices
    CHECK(!Input::IsReplaying());
    CHECK(Input::Down(Action::Left));
}
