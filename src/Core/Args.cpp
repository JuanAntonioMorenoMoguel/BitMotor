#include "BitMotor/Core/Args.h"

#include "BitMotor/Core/Log.h"
#include "Core/ArgsInternal.h"
#include "Core/CommandLine.h"

#include <charconv>
#include <optional>

namespace BitMotor {

namespace {

std::vector<std::string>& Storage() {
    static std::vector<std::string> args = Detail::CommandLine();
    return args;
}

// The option's value, or nullopt if it is missing. A flag without value gives "".
std::optional<std::string> Find(std::string_view name) {
    const auto& args = Storage();
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == name) {
            bool hasValue = i + 1 < args.size() && !(args[i + 1].size() > 1 && args[i + 1].starts_with("--"));
            return hasValue ? args[i + 1] : std::string();
        }
        if (arg.size() > name.size() && arg.starts_with(name) && arg[name.size()] == '=')
            return std::string(arg.substr(name.size() + 1));
    }
    return std::nullopt;
}

template <class T> T Number(std::string_view name, T fallback) {
    auto text = Find(name);
    if (!text) return fallback;
    T value{};
    const char* end = text->data() + text->size();
    auto [stop, error] = std::from_chars(text->data(), end, value);
    if (error == std::errc() && stop == end && !text->empty()) return value;
    Log::Warn("{} needs a number, got '{}'", name, *text);
    return fallback;
}

} // namespace

namespace Detail {
void SetArgs(std::vector<std::string> args) { Storage() = std::move(args); }
} // namespace Detail

namespace Args {

bool Has(std::string_view name) { return Find(name).has_value(); }

std::string Get(std::string_view name, const std::string& fallback) { return Find(name).value_or(fallback); }
std::string Get(std::string_view name, const char* fallback) { return Get(name, std::string(fallback)); }
int Get(std::string_view name, int fallback) { return Number(name, fallback); }
float Get(std::string_view name, float fallback) { return Number(name, fallback); }

const std::vector<std::string>& All() { return Storage(); }

} // namespace Args

} // namespace BitMotor
