// Debug builds: shaders loaded from the .spv files that BitMotor_Shaders writes are recompiled when their source
// changes, with the same compiler the build uses, and the pipelines that use them are remade on their next bind.
#include "Render/GpuInternal.h"

#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "Core/FilesInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <system_error>
#include <vector>

namespace BitMotor::Detail {

namespace {

namespace fs = std::filesystem;

struct WatchedShader {
    fs::path Source; // the GLSL file
    std::weak_ptr<ShaderData> Shader;
};

std::vector<WatchedShader> Watched;
Subscription OnChange;

fs::path Normal(const fs::path& path) {
    std::error_code error;
    fs::path canonical = fs::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

// glslc or glslangValidator, from the Vulkan SDK or the PATH. Empty if there is neither.
fs::path FindCompiler(bool& glslc) {
    std::vector<fs::path> folders;
    if (const char* sdk = std::getenv("VULKAN_SDK")) {
        folders.push_back(fs::path(sdk) / "bin");
        folders.push_back(fs::path(sdk) / "Bin");
    }
    if (const char* path = std::getenv("PATH")) {
#ifdef _WIN32
        constexpr char separator = ';';
        const char* suffix = ".exe";
#else
        constexpr char separator = ':';
        const char* suffix = "";
#endif
        std::string list = path;
        std::size_t start = 0;
        while (start <= list.size()) {
            std::size_t end = list.find(separator, start);
            if (end == std::string::npos) end = list.size();
            if (end > start) folders.emplace_back(list.substr(start, end - start));
            start = end + 1;
        }
        for (const char* name : {"glslc", "glslangValidator"})
            for (const fs::path& folder : folders) {
                std::error_code error;
                fs::path candidate = folder / (std::string(name) + suffix);
                if (fs::is_regular_file(candidate, error)) {
                    glslc = std::string(name) == "glslc";
                    return candidate;
                }
            }
    }
    return {};
}

// Runs the compiler. The compiled code, or empty with the compiler's messages in `output`.
std::vector<std::uint8_t> Compile(const fs::path& source, std::string& output) {
    bool glslc = false;
    const fs::path compiler = FindCompiler(glslc);
    if (compiler.empty()) {
        output = "neither glslc nor glslangValidator is in the PATH (install the Vulkan SDK)";
        return {};
    }
    std::error_code error;
    const fs::path folder = fs::temp_directory_path(error) / "BitMotorShaders";
    fs::create_directories(folder, error);
    const fs::path out = folder / (source.filename().string() + ".spv");
    fs::remove(out, error);
    const std::string compilerText = Utf8(compiler), sourceText = Utf8(source), outText = Utf8(out),
                      include = "-I" + Utf8(source.parent_path());
    std::vector<const char*> args;
    if (glslc)
        args = {compilerText.c_str(), "--target-env=vulkan1.0", "-O", include.c_str(), sourceText.c_str(), "-o",
                outText.c_str()};
    else
        args = {compilerText.c_str(), "-V", "--target-env", "vulkan1.0", include.c_str(),
                sourceText.c_str(),   "-o", outText.c_str()};
    args.push_back(nullptr);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args.data());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        output = SDL_GetError();
        return {};
    }
    std::size_t size = 0;
    int exitCode = -1;
    if (void* text = SDL_ReadProcess(process, &size, &exitCode)) {
        output.assign(static_cast<const char*>(text), size);
        SDL_free(text);
    }
    SDL_DestroyProcess(process);
    if (exitCode != 0) return {};
    auto bytes = Files::ReadBytes(out);
    fs::remove(out, error);
    return bytes ? *bytes : std::vector<std::uint8_t>{};
}

void ReloadSource(const fs::path& path) {
    bool any = false;
    std::vector<std::shared_ptr<ShaderData>> shaders;
    for (const WatchedShader& watched : Watched)
        if (watched.Source == path)
            if (auto shader = watched.Shader.lock()) shaders.push_back(shader);
    if (shaders.empty()) return;
    std::string output;
    std::vector<std::uint8_t> code = Compile(path, output);
    const bool spirv = code.size() >= 20 && code.size() % 4 == 0 && code[0] == 0x03 && code[1] == 0x02 &&
                       code[2] == 0x23 && code[3] == 0x07;
    if (!spirv) {
        Log::Error("Shader {} not reloaded:\n{}", Utf8(path.filename()), output);
        return;
    }
    for (const auto& shader : shaders) {
        shader->Code = code;
        ++shader->Version;
        any = true;
    }
    if (any) Log::Info("Shader reloaded: {}", Utf8(path.filename()));
}

// A shader changed, or a .glsl that the shaders of its folder may include.
void Reload(const fs::path& changed) {
    std::erase_if(Watched, [](const WatchedShader& w) { return w.Shader.expired(); });
    const fs::path path = Normal(changed);
    if (path.extension() != ".glsl") {
        ReloadSource(path);
        return;
    }
    std::vector<fs::path> sources;
    for (const WatchedShader& watched : Watched)
        if (watched.Source.parent_path() == path.parent_path() &&
            std::find(sources.begin(), sources.end(), watched.Source) == sources.end())
            sources.push_back(watched.Source);
    for (const fs::path& source : sources) ReloadSource(source);
}

} // namespace

void WatchShaderSource(const std::shared_ptr<ShaderData>& shader, const fs::path& spv) {
    if (spv.extension() != ".spv") return;
    fs::path relative = spv;
    relative.replace_extension(); // shaders/x.frag.spv -> shaders/x.frag
    const fs::path source = Files::AssetPath(relative);
    if (!Files::Exists(source)) return; // a shipped game: only the .spv is there
    const fs::path normal = Normal(source);
    Watched.push_back({normal, shader});
    Files::Watch(source.parent_path()); // the folder: its .glsl includes count too
    if (!OnChange.Active()) OnChange = Events::On<FileChanged>([](const FileChanged& e) { Reload(e.Path); });
}

} // namespace BitMotor::Detail
