#include "BitMotor/Text/Language.h"

#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Settings.h"
#include "Core/FilesInternal.h"
#include "Text/LanguageInternal.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace BitMotor {

namespace {

using Strings = std::map<std::string, std::string, std::less<>>;

struct LanguageState {
    bool Resolved = false;
    std::string Requested; // what the settings asked for ("" = the system's)
    std::string Code = "en";
    Strings Current, English;
    std::optional<std::filesystem::path> Folder; // tests replace lang/
} State;

std::filesystem::path Folder() { return State.Folder ? *State.Folder : Files::AssetPath("lang"); }

std::filesystem::path FileFor(std::string_view code) {
    return Folder() / Detail::Utf8Path(std::string(code) + ".json");
}

// {"menu": {"play": "Jugar"}} -> "menu.play" = "Jugar". Numbers and booleans are kept as text.
void Flatten(const nlohmann::json& value, const std::string& prefix, Strings& out) {
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it)
            Flatten(it.value(), prefix.empty() ? it.key() : prefix + "." + it.key(), out);
    } else if (value.is_string()) {
        out[prefix] = value.get<std::string>();
    } else if (!value.is_null()) {
        out[prefix] = value.dump();
    }
}

std::optional<Strings> Read(std::string_view code) {
    auto text = Files::Read(FileFor(code));
    if (!text) return std::nullopt;
    nlohmann::json json = nlohmann::json::parse(*text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        Log::Error("lang/{}.json is not a valid JSON object", code);
        return std::nullopt;
    }
    Strings strings;
    Flatten(json, "", strings);
    return strings;
}

bool Available(std::string_view code) { return !code.empty() && Files::Exists(FileFor(code)); }

// The file to use for a request: itself, its base language ("pt-BR" -> "pt"), or nothing.
std::string Match(std::string_view code) {
    if (Available(code)) return std::string(code);
    std::string base(code.substr(0, code.find_first_of("-_")));
    return Available(base) ? base : std::string();
}

std::string SystemLanguage() {
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    std::string found;
    for (int i = 0; locales && i < count && found.empty(); ++i) {
        if (!locales[i] || !locales[i]->language) continue;
        std::string language = locales[i]->language;
        if (locales[i]->country) found = Match(language + "-" + locales[i]->country);
        if (found.empty()) found = Match(language);
    }
    SDL_free(locales);
    return found;
}

void Load(std::string_view requested) {
    State.Requested = requested;
    std::string code = requested.empty() ? SystemLanguage() : Match(requested);
    if (code.empty() && !requested.empty()) Log::Warn("There is no lang/{}.json: using English", requested);
    if (code.empty()) code = "en";
    const bool changed = State.Resolved && code != State.Code;
    State.Code = code;
    State.English = Read("en").value_or(Strings{});
    State.Current = code == "en" ? State.English : Read(code).value_or(Strings{});
    State.Resolved = true;
    if (changed) Events::Emit(LanguageChanged{State.Code});
}

void Ensure() {
    if (!State.Resolved) Load(Settings::Get().Language);
}

// How the language writes numbers: the thousands separator and the decimal mark.
struct NumberMarks {
    const char* Thousands;
    char Decimal;
};

NumberMarks MarksFor(std::string_view code) {
    std::string_view base = code.substr(0, code.find_first_of("-_"));
    for (std::string_view dot : {"es", "de", "it", "pt", "nl", "da", "tr", "id", "el", "ro", "hr", "sl", "sr", "ca"})
        if (base == dot) return {".", ','};
    for (std::string_view space :
         {"fr", "ru", "pl", "cs", "sk", "sv", "fi", "nb", "no", "uk", "hu", "bg", "lt", "lv", "et"})
        if (base == space) return {" ", ','};
    return {",", '.'};
}

} // namespace

namespace Detail {

std::string Translate(std::string_view key, std::initializer_list<std::string> arguments) {
    Ensure();
    std::string text;
    if (auto it = State.Current.find(key); it != State.Current.end())
        text = it->second;
    else if (auto english = State.English.find(key); english != State.English.end())
        text = english->second;
    else
        text = std::string(key);
    std::size_t index = 0;
    for (const std::string& argument : arguments) {
        std::string hole = "{" + std::to_string(index++) + "}";
        for (std::size_t at = text.find(hole); at != std::string::npos; at = text.find(hole, at + argument.size()))
            text.replace(at, hole.size(), argument);
    }
    return text;
}

void ApplyLanguageSetting(const std::string& requested) {
    if (!State.Resolved || requested != State.Requested) Load(requested);
}

void SetLanguageFolder(std::optional<std::filesystem::path> folder) {
    State = LanguageState{};
    State.Folder = std::move(folder);
}

} // namespace Detail

namespace Text {

void SetLanguage(std::string_view code) {
    Settings::Get().Language = code;
    Load(code);
}

const std::string& Language() {
    Ensure();
    return State.Code;
}

std::vector<std::string> Languages() {
    std::vector<std::string> codes;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(Folder(), error))
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            codes.push_back(Detail::Utf8(entry.path().stem()));
    std::sort(codes.begin(), codes.end());
    return codes;
}

std::string LanguageName(std::string_view code) {
    if (auto strings = Read(code))
        if (auto it = strings->find("language"); it != strings->end()) return it->second;
    return std::string(code);
}

std::string Number(double value, int decimals) {
    if (!std::isfinite(value)) return value != value ? "NaN" : value > 0 ? "∞" : "-∞";
    decimals = std::clamp(decimals, 0, 9);
    NumberMarks marks = MarksFor(Language());
    std::string digits = std::format("{:.{}f}", std::abs(value), decimals);
    std::string whole = digits.substr(0, digits.find('.')), fraction;
    if (decimals > 0) fraction = digits.substr(digits.find('.') + 1);
    std::string grouped;
    for (std::size_t i = 0; i < whole.size(); ++i) {
        if (i > 0 && (whole.size() - i) % 3 == 0) grouped += marks.Thousands;
        grouped.push_back(whole[i]);
    }
    bool negative = value < 0 && std::any_of(digits.begin(), digits.end(), [](char c) { return c >= '1' && c <= '9'; });
    std::string out = negative ? "-" + grouped : grouped;
    if (!fraction.empty()) out += marks.Decimal + fraction;
    return out;
}

std::string ShortNumber(double value) {
    if (!std::isfinite(value) || std::abs(value) < 1000) return Number(std::round(value));
    static constexpr const char* Units[] = {"K", "M", "B", "T"};
    double scaled = std::abs(value);
    int unit = -1;
    while (unit < 3 && std::round(scaled * 10) / 10 >= 1000) {
        scaled /= 1000;
        ++unit;
    }
    if (unit < 0) return Number(std::round(value));
    double rounded = std::round(scaled * 10) / 10;
    bool whole = rounded == std::floor(rounded) || rounded >= 100;
    std::string text = Number(value < 0 ? -rounded : rounded, whole ? 0 : 1);
    return text + Units[unit];
}

std::string Percent(double fraction, int decimals) { return Number(fraction * 100, decimals) + "%"; }

std::string Clock(double seconds, bool hundredths) {
    if (!std::isfinite(seconds)) return "-:--";
    bool negative = seconds < 0;
    double total = std::abs(seconds);
    long long centis =
        hundredths ? (long long)std::floor(total * 100 + 1e-6) : (long long)std::floor(total + 1e-6) * 100;
    long long whole = centis / 100;
    long long h = whole / 3600, m = (whole / 60) % 60, s = whole % 60;
    std::string out = negative ? "-" : "";
    out += h > 0 ? std::format("{}:{:02}:{:02}", h, m, s) : std::format("{}:{:02}", m, s);
    if (hundredths) out += std::format(".{:02}", centis % 100);
    return out;
}

} // namespace Text

} // namespace BitMotor
