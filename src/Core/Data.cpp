#include "BitMotor/Core/Data.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "Core/FilesInternal.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>

namespace BitMotor {

using Json = nlohmann::ordered_json;

namespace Detail {

struct DocumentFile {
    std::string Name, Text;
    Json Root;
    bool Ok = true;
};

namespace {

const Json& NodeOf(const void* node) { return *static_cast<const Json*>(node); }

// 1-based line of the first "key" in the text (after the first "context" if given), or 0.
int LineOf(const std::string& text, const std::string& key, const std::string& context) {
    if (key.empty()) return 0;
    std::size_t start = 0;
    if (!context.empty()) {
        std::size_t at = text.find('"' + context + '"');
        if (at != std::string::npos) start = at;
    }
    std::size_t at = text.find('"' + key + '"', start);
    if (at == std::string::npos) at = text.find('"' + key + '"');
    if (at == std::string::npos) return 0;
    return 1 + int(std::count(text.begin(), text.begin() + std::ptrdiff_t(at), '\n'));
}

} // namespace

bool IsHexColor(std::string_view text) {
    if (!text.empty() && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6 && text.size() != 8) return false;
    return std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

std::string DataAccess::ArrayOf(std::size_t count) { return "a list of " + std::to_string(count) + " numbers"; }

std::filesystem::path SaveFilePath(const std::filesystem::path& path) {
    return path.is_absolute() ? path : Files::SavePath(path);
}

bool WriteSaveFile(const std::filesystem::path& path, const std::string& text) {
    return Files::Write(path, text + "\n");
}

} // namespace Detail

// ---------------------------------------------------------------- Document

Document::Document() = default;

Document::Document(Document&& other) noexcept
    : File(std::move(other.File)), Node(std::exchange(other.Node, nullptr)), Path(std::move(other.Path)),
      Key(std::move(other.Key)), ParentKey(std::move(other.ParentKey)), Used(std::move(other.Used)),
      ReadByField(std::exchange(other.ReadByField, false)) {}

Document& Document::operator=(Document&& other) noexcept {
    if (this != &other) {
        ReportUnknown();
        File = std::move(other.File);
        Node = std::exchange(other.Node, nullptr);
        Path = std::move(other.Path);
        Key = std::move(other.Key);
        ParentKey = std::move(other.ParentKey);
        Used = std::move(other.Used);
        ReadByField = std::exchange(other.ReadByField, false);
    }
    return *this;
}

Document::~Document() { ReportUnknown(); }

bool Document::Ok() const { return File && File->Ok; }

const std::string& Document::Name() const {
    static const std::string none;
    return File ? File->Name : none;
}

bool Document::Has(std::string_view key) const {
    if (!Node) return false;
    const Json& json = Detail::NodeOf(Node);
    return json.is_object() && json.contains(std::string(key));
}

Document::Kind Document::Type() const {
    if (!Node) return Kind::Missing;
    const Json& json = Detail::NodeOf(Node);
    switch (json.type()) {
    case Json::value_t::null:
        return Kind::Null;
    case Json::value_t::boolean:
        return Kind::Bool;
    case Json::value_t::number_integer:
        return Kind::Integer;
    case Json::value_t::number_unsigned:
        return Kind::Unsigned;
    case Json::value_t::number_float:
        return Kind::Float;
    case Json::value_t::string:
        return Kind::String;
    case Json::value_t::array:
        return Kind::Array;
    case Json::value_t::object:
        return Kind::Object;
    default:
        return Kind::Null;
    }
}

bool Document::AsBool() const { return Detail::NodeOf(Node).get<bool>(); }
long long Document::AsInteger() const { return Detail::NodeOf(Node).get<long long>(); }
unsigned long long Document::AsUnsigned() const { return Detail::NodeOf(Node).get<unsigned long long>(); }
double Document::AsNumber() const { return Detail::NodeOf(Node).get<double>(); }
std::string Document::AsString() const { return Detail::NodeOf(Node).get<std::string>(); }

std::size_t Document::Size() const {
    if (!Node) return 0;
    const Json& json = Detail::NodeOf(Node);
    return json.is_array() || json.is_object() ? json.size() : 0;
}

Document Document::Element(std::size_t index) const {
    Document element;
    element.File = File;
    element.Path = Path + "[" + std::to_string(index) + "]";
    element.Key = Key;
    element.ParentKey = ParentKey;
    if (Node && Detail::NodeOf(Node).is_array() && index < Detail::NodeOf(Node).size())
        element.Node = &Detail::NodeOf(Node)[index];
    return element;
}

Document Document::Member(std::string_view key) {
    std::string name(key);
    if (std::find(Used.begin(), Used.end(), name) == Used.end()) Used.push_back(name);
    ReadByField = true;
    Document member;
    member.File = File;
    member.Path = Path.empty() ? name : Path + "." + name;
    member.Key = name;
    member.ParentKey = Key;
    if (Node) {
        const Json& json = Detail::NodeOf(Node);
        if (json.is_object()) {
            auto it = json.find(name);
            if (it != json.end()) member.Node = &*it;
        }
    }
    return member;
}

Document Document::Child(std::string_view key) {
    Document child = Member(key);
    if (child.Type() != Kind::Missing && child.Type() != Kind::Object) child.Mismatch("an object {...}");
    return child;
}

std::vector<std::string> Document::Keys() {
    std::vector<std::string> keys;
    if (!Node || !Detail::NodeOf(Node).is_object()) return keys;
    for (auto& item : Detail::NodeOf(Node).items()) keys.push_back(item.key());
    for (const auto& key : keys)
        if (std::find(Used.begin(), Used.end(), key) == Used.end()) Used.push_back(key);
    return keys;
}

std::vector<Document> Document::Items() {
    std::vector<Document> items;
    if (Type() == Kind::Array) {
        for (std::size_t i = 0; i < Size(); ++i) items.push_back(Element(i));
    } else if (Type() == Kind::Object) {
        for (const std::string& key : Keys()) items.push_back(Member(key));
    }
    return items;
}

void Document::Error(std::string_view key, std::string_view message) { Report(true, key, message); }
void Document::Warn(std::string_view key, std::string_view message) { Report(false, key, message); }

void Document::Report(bool error, std::string_view key, std::string_view message) {
    std::string name = File ? File->Name : std::string("(no file)");
    int line =
        File ? Detail::LineOf(File->Text, key.empty() ? Key : std::string(key), key.empty() ? ParentKey : Key) : 0;
    std::string where = line > 0 ? name + ":" + std::to_string(line) : name;
    if (error) {
        if (File) File->Ok = false;
        Log::Error("{}: {}", where, message);
    } else {
        Log::Warn("{}: {}", where, message);
    }
}

bool Document::Mismatch(std::string_view expected) {
    std::string field = Path.empty() ? std::string("the root") : "'" + Path + "'";
    Report(true, "", field + " should be " + std::string(expected));
    return false;
}

void Document::ReportUnknown() {
    if (!ReadByField || !Node || !File) return;
    const Json& json = Detail::NodeOf(Node);
    if (!json.is_object()) return;
    for (auto& item : json.items()) {
        if (std::find(Used.begin(), Used.end(), item.key()) != Used.end()) continue;
        std::string in = Path.empty() ? std::string() : " in '" + Path + "'";
        Report(false, item.key(), "unknown field '" + item.key() + "'" + in + " (typo?)");
    }
    ReadByField = false;
}

// ---------------------------------------------------------------- opening

Document ParseDocument(std::string_view name, std::string_view text) {
    auto file = std::make_shared<Detail::DocumentFile>();
    file->Name = name;
    file->Text = text;
    Document document;
    document.File = file;
    try {
        file->Root = Json::parse(file->Text);
        document.Node = &file->Root;
    } catch (const Json::parse_error& error) {
        file->Ok = false;
        // nlohmann reports a byte offset: turn it into a line.
        std::size_t offset = std::min<std::size_t>(error.byte, file->Text.size());
        int line = 1 + int(std::count(file->Text.begin(), file->Text.begin() + std::ptrdiff_t(offset), '\n'));
        std::string what = error.what();
        auto colon = what.rfind(": ");
        Log::Error("{}:{}: broken JSON: {}", file->Name, line,
                   colon != std::string::npos ? what.substr(colon + 2) : what);
    }
    return document;
}

namespace Detail {

Document OpenFile(const std::filesystem::path& path, bool reportMissing) {
    std::filesystem::path full = path.is_absolute() ? path : Files::AssetPath(path);
    auto text = Files::Read(full);
    if (!text) {
        if (reportMissing) Log::Error("cannot open '{}'", Utf8(full));
        Document missing;
        missing.File = std::make_shared<DocumentFile>();
        missing.File->Name = Utf8(path.filename());
        missing.File->Ok = false;
        return missing;
    }
    return ParseDocument(Utf8(path.filename()), *text);
}

// ---------------------------------------------------------------- JsonWriter

struct JsonWriter::State {
    Json Root;
    std::vector<Json*> Stack;
    std::string PendingKey;
    bool HasKey = false;

    void Add(Json value) {
        if (Stack.empty()) {
            Root = std::move(value);
            if (Root.is_structured()) Stack.push_back(&Root);
            return;
        }
        Json& parent = *Stack.back();
        Json* added = nullptr;
        if (parent.is_array()) {
            parent.push_back(std::move(value));
            added = &parent.back();
        } else {
            added = &(parent[PendingKey] = std::move(value));
            HasKey = false;
        }
        if (added->is_structured()) Stack.push_back(added);
    }
};

JsonWriter::JsonWriter() : Impl(std::make_unique<State>()) {}
JsonWriter::~JsonWriter() = default;

void JsonWriter::Null() { Impl->Add(nullptr); }
void JsonWriter::Bool(bool value) { Impl->Add(value); }
void JsonWriter::Integer(long long value) { Impl->Add(value); }
void JsonWriter::Unsigned(unsigned long long value) { Impl->Add(value); }
void JsonWriter::Number(double value) { Impl->Add(std::isfinite(value) ? Json(value) : Json(nullptr)); }
void JsonWriter::Number(float value) {
    if (!std::isfinite(value)) return Impl->Add(nullptr);
    // Shortest decimal for the float, read back as double: JSON shows 0.1 instead of 0.10000000149011612.
    char text[32];
    auto result = std::to_chars(text, text + sizeof(text), value);
    double asDouble = value;
    std::from_chars(text, result.ptr, asDouble);
    Impl->Add(asDouble);
}
void JsonWriter::String(std::string_view value) { Impl->Add(std::string(value)); }
void JsonWriter::BeginArray() { Impl->Add(Json::array()); }
void JsonWriter::BeginObject() { Impl->Add(Json::object()); }
void JsonWriter::EndArray() {
    if (!Impl->Stack.empty()) Impl->Stack.pop_back();
}
void JsonWriter::EndObject() {
    if (!Impl->Stack.empty()) Impl->Stack.pop_back();
}
void JsonWriter::Key(std::string_view key) {
    Impl->PendingKey = key;
    Impl->HasKey = true;
}

namespace {

void Print(const Json& value, int indent, std::string& out) {
    auto newline = [&](int level) {
        out += '\n';
        out.append(std::size_t(level) * 2, ' ');
    };
    if (value.is_object()) {
        if (value.empty()) {
            out += "{}";
            return;
        }
        out += '{';
        bool first = true;
        for (auto& item : value.items()) {
            if (!first) out += ',';
            first = false;
            newline(indent + 1);
            out += Json(item.key()).dump();
            out += ": ";
            Print(item.value(), indent + 1, out);
        }
        newline(indent);
        out += '}';
    } else if (value.is_array()) {
        bool flat = std::none_of(value.begin(), value.end(), [](const Json& v) { return v.is_structured(); });
        if (value.empty() || flat) {
            out += '[';
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (i > 0) out += ", ";
                out += value[i].dump(-1, ' ', false, Json::error_handler_t::replace);
            }
            out += ']';
            return;
        }
        out += '[';
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i > 0) out += ',';
            newline(indent + 1);
            Print(value[i], indent + 1, out);
        }
        newline(indent);
        out += ']';
    } else {
        out += value.dump(-1, ' ', false, Json::error_handler_t::replace);
    }
}

} // namespace

std::string JsonWriter::Text() const {
    std::string out;
    Print(Impl->Root, 0, out);
    return out;
}

} // namespace Detail

} // namespace BitMotor
