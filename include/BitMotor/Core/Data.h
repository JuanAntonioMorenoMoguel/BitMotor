#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Core/Fields.h"
#include "BitMotor/Math/Vector.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// JSON data files with useful errors, and saves of whole structs.
//
// Content written by hand (relative paths are looked for with Files::AssetPath):
//     Document heroes = Data::Open("data/heroes.json");
//     for (const std::string& id : heroes.Keys()) {
//         Document hero = heroes.Child(id);
//         float health = hero.Get("Health", 10.0f);
//         std::string name = hero.Get<std::string>("Name", id);
//     }   // fields nobody read are reported here as unknown (typos), with file and line
//     if (!heroes.Ok()) ...   // something had the wrong type: already in the log
//
// Saves (relative paths go to Files::SavePath), using BITMOTOR_FIELDS:
//     Data::Save("progress.json", progress);
//     Progress progress = Data::Load<Progress>("progress.json").value_or(Progress{});
// Fields missing in the file keep their default value, so adding fields never breaks old saves.
namespace BitMotor {

class Document;

namespace Detail {
struct DocumentFile;
struct DataAccess;
template <class T> struct AlwaysFalse : std::false_type {};

template <class T>
concept HasFields = requires(T& value) { BitMotorFields(value, [](const char*, auto&) {}); };

template <class T> struct IsVector : std::false_type {};
template <class T, class A> struct IsVector<std::vector<T, A>> : std::true_type {};
template <class T> struct IsOptional : std::false_type {};
template <class T> struct IsOptional<std::optional<T>> : std::true_type {};
template <class T> struct IsStringMap : std::false_type {};
template <class T, class C, class A> struct IsStringMap<std::map<std::string, T, C, A>> : std::true_type {};
template <class T, class H, class E, class A> struct IsStringMap<std::unordered_map<std::string, T, H, E, A>>
    : std::true_type {};

// Builds JSON text. Used by Data::Save; kept free of the JSON library.
class JsonWriter {
public:
    JsonWriter();
    ~JsonWriter();
    JsonWriter(const JsonWriter&) = delete;
    JsonWriter& operator=(const JsonWriter&) = delete;

    void Null();
    void Bool(bool value);
    void Integer(long long value);
    void Unsigned(unsigned long long value);
    void Number(double value);
    void Number(float value); // shortest text that reads back as the same float: 0.1f is "0.1"
    void String(std::string_view value);
    void BeginArray();
    void EndArray();
    void BeginObject();
    void Key(std::string_view key);
    void EndObject();
    // Indented with 2 spaces; arrays of plain values go on one line.
    std::string Text() const;

private:
    struct State;
    std::unique_ptr<State> Impl;
};

Document OpenFile(const std::filesystem::path& path, bool reportMissing);
std::filesystem::path SaveFilePath(const std::filesystem::path& path);
bool WriteSaveFile(const std::filesystem::path& path, const std::string& text);
bool IsHexColor(std::string_view text);

} // namespace Detail

// A JSON value being read: a whole file or a part of it. Move-only.
class Document {
public:
    Document();
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    // Reports the fields of this object that nobody read (only if some field was read).
    ~Document();

    // The file was read and everything read so far had the right type.
    bool Ok() const;
    // File name, for messages.
    const std::string& Name() const;

    bool Has(std::string_view key) const;

    // The field's value, or `fallback` if it is missing. A value of the wrong type logs an error (file,
    // line and field), marks the document as not Ok and returns `fallback`.
    template <class T> T Get(std::string_view key, T fallback = T{});
    std::string Get(std::string_view key, const char* fallback) { return Get<std::string>(key, fallback); }

    // A sub-object to read field by field. Missing: an empty Document (Has is false, Get gives fallbacks).
    Document Child(std::string_view key);
    // Names of this object's fields, in file order. Counts all of them as read.
    std::vector<std::string> Keys();
    // Elements of this array (or values of this object), to read one by one.
    std::vector<Document> Items();

    // Reports a problem with a field (unknown id, value out of range...) with file and line.
    void Error(std::string_view key, std::string_view message);
    void Warn(std::string_view key, std::string_view message);

private:
    friend struct Detail::DataAccess;
    friend Document Detail::OpenFile(const std::filesystem::path& path, bool reportMissing);
    friend Document ParseDocument(std::string_view name, std::string_view text);

    enum class Kind { Missing, Null, Bool, Integer, Unsigned, Float, String, Array, Object };
    Kind Type() const;
    bool AsBool() const;
    long long AsInteger() const;
    unsigned long long AsUnsigned() const;
    double AsNumber() const;
    std::string AsString() const;
    std::size_t Size() const;
    Document Element(std::size_t index) const;
    Document Member(std::string_view key);
    bool Mismatch(std::string_view expected);
    void Report(bool error, std::string_view key, std::string_view message);
    void ReportUnknown();

    std::shared_ptr<Detail::DocumentFile> File;
    const void* Node = nullptr; // the JSON value inside File (nullptr: missing)
    std::string Path;           // "goblin.Stats" (for messages)
    std::string Key, ParentKey; // to find the line in the text
    std::vector<std::string> Used;
    bool ReadByField = false;
};

// Reads a document from text in memory (tests, embedded data). `name` appears in the messages.
Document ParseDocument(std::string_view name, std::string_view text);

namespace Detail {

struct DataAccess {
    template <class T> static bool Read(Document& node, T& out) {
        using Kind = Document::Kind;
        const Kind kind = node.Type();
        if constexpr (std::is_same_v<T, bool>) {
            if (kind != Kind::Bool) return node.Mismatch("true or false");
            out = node.AsBool();
        } else if constexpr (std::is_enum_v<T>) {
            std::underlying_type_t<T> raw{};
            if (!Read(node, raw)) return false;
            out = T(raw);
        } else if constexpr (std::is_integral_v<T>) {
            if (kind == Kind::Integer) {
                long long value = node.AsInteger();
                if constexpr (std::is_signed_v<T>) {
                    if (value < (long long)std::numeric_limits<T>::min() ||
                        value > (long long)std::numeric_limits<T>::max())
                        return node.Mismatch("a smaller whole number");
                } else {
                    if (value < 0 || (unsigned long long)value > std::numeric_limits<T>::max())
                        return node.Mismatch("a positive whole number in range");
                }
                out = T(value);
            } else if (kind == Kind::Unsigned) {
                unsigned long long value = node.AsUnsigned();
                if (value > (unsigned long long)std::numeric_limits<T>::max())
                    return node.Mismatch("a smaller whole number");
                out = T(value);
            } else {
                return node.Mismatch("a whole number");
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            if (kind != Kind::Integer && kind != Kind::Unsigned && kind != Kind::Float)
                return node.Mismatch("a number");
            out = T(node.AsNumber());
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (kind != Kind::String) return node.Mismatch("text");
            out = node.AsString();
        } else if constexpr (std::is_same_v<T, Vec2> || std::is_same_v<T, Vec3> || std::is_same_v<T, Vec4>) {
            constexpr std::size_t count = sizeof(T) / sizeof(float);
            if (kind != Kind::Array || node.Size() != count) return node.Mismatch(ArrayOf(count));
            float values[4] = {};
            for (std::size_t i = 0; i < count; ++i) {
                Document item = node.Element(i);
                if (!Read(item, values[i])) return false;
            }
            if constexpr (count == 2)
                out = {values[0], values[1]};
            else if constexpr (count == 3)
                out = {values[0], values[1], values[2]};
            else
                out = {values[0], values[1], values[2], values[3]};
        } else if constexpr (std::is_same_v<T, Color>) {
            if (kind == Kind::String) {
                std::string text = node.AsString();
                if (!IsHexColor(text)) return node.Mismatch("a colour like \"#RRGGBB\" or \"#RRGGBBAA\"");
                out = Color::Hex(std::string_view(text));
            } else if (kind == Kind::Array && (node.Size() == 3 || node.Size() == 4)) {
                float values[4] = {0, 0, 0, 1};
                for (std::size_t i = 0; i < node.Size(); ++i) {
                    Document item = node.Element(i);
                    if (!Read(item, values[i])) return false;
                }
                out = {values[0], values[1], values[2], values[3]};
            } else {
                return node.Mismatch("a colour like \"#RRGGBB\" or [r, g, b, a]");
            }
        } else if constexpr (IsOptional<T>::value) {
            if (kind == Kind::Null || kind == Kind::Missing) {
                out.reset();
            } else {
                typename T::value_type value = out.value_or(typename T::value_type{});
                if (!Read(node, value)) return false;
                out = std::move(value);
            }
        } else if constexpr (IsVector<T>::value) {
            if (kind != Kind::Array) return node.Mismatch("a list [...]");
            T result;
            result.reserve(node.Size());
            for (std::size_t i = 0; i < node.Size(); ++i) {
                Document item = node.Element(i);
                typename T::value_type value{};
                if (Read(item, value)) result.push_back(std::move(value));
            }
            out = std::move(result);
        } else if constexpr (IsStringMap<T>::value) {
            if (kind != Kind::Object) return node.Mismatch("an object {...}");
            T result;
            for (const std::string& key : node.Keys()) {
                Document item = node.Member(key);
                typename T::mapped_type value{};
                if (Read(item, value)) result.emplace(key, std::move(value));
            }
            out = std::move(result);
        } else if constexpr (HasFields<T>) {
            if (kind != Kind::Object) return node.Mismatch("an object {...}");
            node.ReadByField = true;
            BitMotorFields(out, [&node](const char* name, auto& field) {
                Document member = node.Member(name);
                if (member.Type() != Kind::Missing) Read(member, field); // a bad field keeps its default
            });
        } else {
            static_assert(AlwaysFalse<T>::value, "Data cannot read this type: give it BITMOTOR_FIELDS(Type, ...)");
        }
        return true;
    }

    template <class T> static void Write(JsonWriter& out, const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            out.Bool(value);
        } else if constexpr (std::is_enum_v<T>) {
            Write(out, static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            out.Integer((long long)value);
        } else if constexpr (std::is_integral_v<T>) {
            out.Unsigned((unsigned long long)value);
        } else if constexpr (std::is_same_v<T, float>) {
            out.Number(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            out.Number(double(value));
        } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {
            out.String(value);
        } else if constexpr (std::is_same_v<T, Vec2>) {
            WriteFloats(out, {value.X, value.Y});
        } else if constexpr (std::is_same_v<T, Vec3>) {
            WriteFloats(out, {value.X, value.Y, value.Z});
        } else if constexpr (std::is_same_v<T, Vec4>) {
            WriteFloats(out, {value.X, value.Y, value.Z, value.W});
        } else if constexpr (std::is_same_v<T, Color>) {
            out.String(value.ToHex());
        } else if constexpr (IsOptional<T>::value) {
            if (value)
                Write(out, *value);
            else
                out.Null();
        } else if constexpr (IsVector<T>::value) {
            out.BeginArray();
            for (const auto& item : value) Write(out, item);
            out.EndArray();
        } else if constexpr (IsStringMap<T>::value) {
            out.BeginObject();
            for (const auto& [key, item] : value) {
                out.Key(key);
                Write(out, item);
            }
            out.EndObject();
        } else if constexpr (HasFields<T>) {
            out.BeginObject();
            BitMotorFields(value, [&out](const char* name, const auto& field) {
                out.Key(name);
                Write(out, field);
            });
            out.EndObject();
        } else {
            static_assert(AlwaysFalse<T>::value, "Data cannot write this type: give it BITMOTOR_FIELDS(Type, ...)");
        }
    }

private:
    static std::string ArrayOf(std::size_t count);
    static void WriteFloats(JsonWriter& out, std::initializer_list<float> values) {
        out.BeginArray();
        for (float v : values) out.Number(v);
        out.EndArray();
    }
};

} // namespace Detail

template <class T> T Document::Get(std::string_view key, T fallback) {
    Document member = Member(key);
    if (member.Type() == Kind::Missing) return fallback;
    T value = fallback;
    return Detail::DataAccess::Read(member, value) ? value : fallback;
}

namespace Data {

using Path = std::filesystem::path;

// Opens a JSON file to read it field by field. Relative paths: Files::AssetPath. A missing file or broken
// JSON logs an error and gives a Document that is not Ok (every Get returns its fallback).
inline Document Open(const Path& path) { return Detail::OpenFile(path, true); }

// Reads a whole struct saved with Save. Relative paths: Files::SavePath. nullopt if the file does not
// exist (first run) or is not valid JSON (logged).
template <class T> std::optional<T> Load(const Path& path) {
    Document document = Detail::OpenFile(Detail::SaveFilePath(path), false);
    if (!document.Ok()) return std::nullopt;
    T value{};
    if (!Detail::DataAccess::Read(document, value)) return std::nullopt;
    return value;
}

// Writes a whole struct as JSON, atomically. Relative paths: Files::SavePath. false if it fails (logged).
template <class T> bool Save(const Path& path, const T& value) {
    Detail::JsonWriter writer;
    Detail::DataAccess::Write(writer, value);
    return Detail::WriteSaveFile(Detail::SaveFilePath(path), writer.Text());
}

// The same conversions in memory.
template <class T> std::string ToJson(const T& value) {
    Detail::JsonWriter writer;
    Detail::DataAccess::Write(writer, value);
    return writer.Text();
}
template <class T> std::optional<T> FromJson(std::string_view text, std::string_view name = "json") {
    Document document = ParseDocument(name, text);
    if (!document.Ok()) return std::nullopt;
    T value{};
    if (!Detail::DataAccess::Read(document, value)) return std::nullopt;
    return value;
}

} // namespace Data

} // namespace BitMotor
