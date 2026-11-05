#include "BitMotor/Core/Files.h"

#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Game.h"
#include "BitMotor/Core/Log.h"
#include "Core/FilesInternal.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <map>
#include <system_error>

namespace BitMotor {

namespace fs = std::filesystem;

namespace {

std::string SourceDir;
std::string Organization = "BitMotor", Application = "Game";
fs::path SaveFolder;

struct Watched {
    fs::path Root;
    std::map<fs::path, fs::file_time_type> Times;
};
std::vector<Watched> Watches;
double WatchClock = 0;

template <class Container> std::optional<Container> ReadAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    Container data;
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size < 0) return std::nullopt;
    data.resize(std::size_t(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), std::streamsize(size));
    if (!file && !file.eof()) return std::nullopt;
    return data;
}

bool WriteAtomic(const fs::path& path, const char* data, std::size_t size) {
    std::error_code error;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), error);
    fs::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            Log::Error("cannot write '{}'", Detail::Utf8(path));
            return false;
        }
        file.write(data, std::streamsize(size));
        file.flush();
        if (!file) {
            Log::Error("cannot write '{}' (disk full?)", Detail::Utf8(path));
            file.close();
            fs::remove(temporary, error);
            return false;
        }
    }
    fs::rename(temporary, path, error);
    if (error) {
        Log::Error("cannot replace '{}': {}", Detail::Utf8(path), error.message());
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

// Every regular file under the root (or the root itself if it is a file), with its modification time.
std::map<fs::path, fs::file_time_type> Scan(const fs::path& root) {
    std::map<fs::path, fs::file_time_type> times;
    std::error_code error;
    if (fs::is_regular_file(root, error)) {
        times[root] = fs::last_write_time(root, error);
        return times;
    }
    if (!fs::is_directory(root, error)) return times;
    for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_regular_file(error)) continue;
        auto time = it->last_write_time(error);
        if (!error) times[it->path()] = time;
    }
    return times;
}

fs::path ExecutableDir() {
    const char* base = SDL_GetBasePath();
    return base ? Detail::Utf8Path(base) : fs::current_path();
}

} // namespace

namespace Detail {

void SetSourceDir(const char* folder) { SourceDir = folder ? folder : ""; }

void SetIdentity(const std::string& organization, const std::string& application) {
    Organization = organization;
    Application = application;
    SaveFolder.clear();
}

void SetSavePath(const fs::path& folder) { SaveFolder = folder; }

void UpdateWatches(double dt, bool force) {
    WatchClock += dt;
    if (!force && WatchClock < 0.5) return;
    WatchClock = 0;
    for (auto& watch : Watches) {
        auto now = Scan(watch.Root);
        for (auto& [path, time] : now) {
            auto old = watch.Times.find(path);
            if (old == watch.Times.end() || old->second != time) Events::Queue(FileChanged{path});
        }
        watch.Times = std::move(now);
    }
}

} // namespace Detail

namespace Files {

std::optional<std::string> Read(const Path& path) { return ReadAll<std::string>(path); }

std::optional<std::vector<std::uint8_t>> ReadBytes(const Path& path) {
    return ReadAll<std::vector<std::uint8_t>>(path);
}

bool Write(const Path& path, std::string_view text) { return WriteAtomic(path, text.data(), text.size()); }

bool WriteBytes(const Path& path, std::span<const std::uint8_t> bytes) {
    return WriteAtomic(path, reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool Exists(const Path& path) {
    std::error_code error;
    return fs::exists(path, error);
}

Path AssetPath(const Path& relative) {
    if (!SourceDir.empty()) {
        Path inSource = Detail::Utf8Path(SourceDir) / relative;
        if (relative.empty() || Exists(inSource)) return inSource;
    }
    return ExecutableDir() / relative;
}

Path SavePath(const Path& relative) {
    if (SaveFolder.empty()) {
        char* pref = SDL_GetPrefPath(Organization.c_str(), Application.c_str());
        if (pref) {
            SaveFolder = Detail::Utf8Path(pref);
            SDL_free(pref);
        } else {
            Log::Error("no save folder ({}), using the executable's folder", SDL_GetError());
            SaveFolder = ExecutableDir() / "saves";
        }
    }
    std::error_code error;
    fs::create_directories(SaveFolder, error);
    return SaveFolder / relative;
}

void Watch(const Path& path) {
    for (auto& watch : Watches)
        if (watch.Root == path) return;
    Watches.push_back({path, Scan(path)});
}

void Unwatch(const Path& path) {
    std::erase_if(Watches, [&](const Watched& watch) { return watch.Root == path; });
}

} // namespace Files

} // namespace BitMotor
