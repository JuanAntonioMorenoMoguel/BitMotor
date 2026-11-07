#pragma once
#include <format>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

// Translations and numbers in the player's language. Each language is a JSON file in the game's assets,
// lang/<code>.json, with plain or nested keys:
//     lang/es.json: {"menu": {"play": "Jugar", "quit": "Salir"}, "score": "Puntos: {0}", "hello": "Hola, {0}"}
//
//     Text::Draw(Tr("menu.play"), {10, 10});
//     Text::Draw(Tr("score", Text::Number(points)), {10, 20});
//     Text::SetLanguage("en");       // also Settings::Language, without saving
//
// The language comes from Settings::Language, or the system's when it is empty (falling back to English).
// A key missing in that language is looked for in English (lang/en.json), and if it is not there either,
// Tr returns the key itself, so a missing translation shows up but never breaks anything.
namespace BitMotor {

// Sent after the language changes (Text::SetLanguage, or Settings::Apply with another Settings::Language).
struct LanguageChanged {
    std::string Language;
};

namespace Detail {
std::string Translate(std::string_view key, std::initializer_list<std::string> arguments);
}

// The text for `key` in the current language; {0}, {1}... are replaced by the arguments.
inline std::string Tr(std::string_view key) { return Detail::Translate(key, {}); }
template <class... A> std::string Tr(std::string_view key, const A&... arguments) {
    return Detail::Translate(key, {std::format("{}", arguments)...});
}

namespace Text {

// "es", "en", "pt-BR"... (the name of a file in lang/). Empty: the system's language.
void SetLanguage(std::string_view code);
// The language in use (the file that was found; "en" when there is none).
const std::string& Language();
// The languages the game has: the files in lang/, sorted ("en", "es"...).
std::vector<std::string> Languages();
// The language's own name, from the "language" key of its file ("Español"); the code if it has none.
std::string LanguageName(std::string_view code);

// Numbers written the way the language does: Number(12500) is "12,500" in English, "12.500" in Spanish.
std::string Number(double value, int decimals = 0);
// Big numbers shortened: 950 -> "950", 12500 -> "12.5K", 3400000 -> "3.4M" (K, M, B, T).
std::string ShortNumber(double value);
// 0.42 -> "42%".
std::string Percent(double fraction, int decimals = 0);
// Games written in their own language that translate by the original text (gettext style) instead of
// by keys: every plain text that Text::Draw, Measure, Wrap, Layout, Paragraph and the UI widgets show goes
// through `translate` first. Rich markup is left as it is. Empty function: no translation.
//     Text::SetTranslator([&](std::string_view text) { return catalog.Lookup(text); });
void SetTranslator(std::function<std::string(std::string_view)> translate);
// Seconds as a clock: 65 -> "1:05", 3725 -> "1:02:05"; with hundredths, 65.32 -> "1:05.32".
std::string Clock(double seconds, bool hundredths = false);

} // namespace Text

} // namespace BitMotor
