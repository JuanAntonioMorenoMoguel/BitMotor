#include "Testing.h"

#include "Core/EventsInternal.h"
#include "Core/FilesInternal.h"

#include <BitMotor/BitMotor.h>

#include <filesystem>
#include <thread>

using namespace BitMotor;
namespace fs = std::filesystem;

namespace {
fs::path TestFolder(const char* name) {
    fs::path folder = fs::temp_directory_path() / "BitMotorTests" / name;
    fs::remove_all(folder);
    return folder;
}
} // namespace

TEST(FilesWriteAndReadBack) {
    fs::path folder = TestFolder("Files");
    fs::path file = folder / "deep" / "inside" / "notes.txt"; // folders are created
    CHECK(!Files::Exists(file));
    CHECK(!Files::Read(file).has_value());
    CHECK(Files::Write(file, "line 1\r\nline 2\n"));
    CHECK(Files::Exists(file));
    CHECK_EQ(Files::Read(file).value_or(""), std::string("line 1\r\nline 2\n")); // byte by byte
    CHECK(Files::Write(file, "replaced"));
    CHECK_EQ(Files::Read(file).value_or(""), std::string("replaced"));
    CHECK(!Files::Exists(fs::path(file) += ".tmp")); // no temporary left behind

    std::vector<std::uint8_t> bytes{0, 1, 2, 255, 0, 10};
    CHECK(Files::WriteBytes(folder / "data.bin", bytes));
    CHECK(Files::ReadBytes(folder / "data.bin").value_or(std::vector<std::uint8_t>{}) == bytes);
    CHECK(Files::Write(folder / "empty.txt", ""));
    CHECK_EQ(Files::Read(folder / "empty.txt").value_or("x"), std::string());
}

TEST(FilesSavePathUsesTheChosenFolder) {
    fs::path folder = TestFolder("Saves");
    Detail::SetSavePath(folder);
    CHECK(Files::SavePath("slot1.json") == folder / "slot1.json");
    CHECK(fs::is_directory(folder));
}

TEST(FilesWatchSendsFileChanged) {
    fs::path folder = TestFolder("Watch");
    CHECK(Files::Write(folder / "a.txt", "1"));
    Files::Watch(folder);
    std::vector<fs::path> changed;
    Subscription s = Events::On<FileChanged>([&](const FileChanged& e) { changed.push_back(e.Path); });

    Detail::UpdateWatches(0, true);
    Detail::FlushEvents();
    CHECK(changed.empty()); // nothing changed yet

    CHECK(Files::Write(folder / "sub" / "b.txt", "new file"));
    // Make sure a.txt gets a different time even on file systems with coarse timestamps.
    fs::last_write_time(folder / "a.txt", fs::last_write_time(folder / "a.txt") + std::chrono::seconds(2));
    Detail::UpdateWatches(0.1); // less than half a second: not checked yet
    Detail::FlushEvents();
    CHECK(changed.empty());
    Detail::UpdateWatches(0.5);
    Detail::FlushEvents();
    CHECK_EQ(changed.size(), std::size_t(2));

    Files::Unwatch(folder);
    CHECK(Files::Write(folder / "c.txt", "ignored"));
    Detail::UpdateWatches(0, true);
    Detail::FlushEvents();
    CHECK_EQ(changed.size(), std::size_t(2));
}
