#include "storage.h"

#include <windows.h>

#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>

namespace {
struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object> value;
};

class JsonParser {
public:
    explicit JsonParser(const std::string_view input) : input_(input) {}

    JsonValue parse() {
        JsonValue result = parse_value();
        skip_whitespace();
        if (position_ != input_.size()) {
            fail("unexpected trailing content");
        }
        return result;
    }

private:
    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error("Invalid medication JSON at byte " + std::to_string(position_) + ": " + message);
    }

    void skip_whitespace() {
        while (position_ < input_.size() &&
               (input_[position_] == ' ' || input_[position_] == '\n' || input_[position_] == '\r' ||
                input_[position_] == '\t')) {
            ++position_;
        }
    }

    bool consume(const char expected) {
        skip_whitespace();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    JsonValue parse_value() {
        skip_whitespace();
        if (position_ == input_.size()) {
            fail("expected a value");
        }

        switch (input_[position_]) {
        case '{': return JsonValue{parse_object()};
        case '[': return JsonValue{parse_array()};
        case '"': return JsonValue{parse_string()};
        case 't': parse_literal("true"); return JsonValue{true};
        case 'f': parse_literal("false"); return JsonValue{false};
        case 'n': parse_literal("null"); return JsonValue{nullptr};
        default: return JsonValue{parse_integer()};
        }
    }

    JsonValue::Object parse_object() {
        ++position_;
        JsonValue::Object object;
        if (consume('}')) {
            return object;
        }

        while (true) {
            skip_whitespace();
            if (position_ == input_.size() || input_[position_] != '"') {
                fail("expected an object key");
            }
            std::string key = parse_string();
            if (!consume(':')) {
                fail("expected ':'");
            }
            if (!object.emplace(std::move(key), parse_value()).second) {
                fail("duplicate object key");
            }
            if (consume('}')) {
                return object;
            }
            if (!consume(',')) {
                fail("expected ',' or '}'");
            }
        }
    }

    JsonValue::Array parse_array() {
        ++position_;
        JsonValue::Array array;
        if (consume(']')) {
            return array;
        }

        while (true) {
            array.push_back(parse_value());
            if (consume(']')) {
                return array;
            }
            if (!consume(',')) {
                fail("expected ',' or ']'");
            }
        }
    }

    static void append_utf8(std::string& output, const std::uint32_t code_point) {
        if (code_point <= 0x7f) {
            output.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
            output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
            output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
    }

    std::uint32_t parse_hex_quad() {
        if (input_.size() - position_ < 4) {
            fail("incomplete Unicode escape");
        }
        std::uint32_t value{};
        for (int i = 0; i < 4; ++i) {
            const char character = input_[position_++];
            value <<= 4;
            if (character >= '0' && character <= '9') value += character - '0';
            else if (character >= 'a' && character <= 'f') value += character - 'a' + 10;
            else if (character >= 'A' && character <= 'F') value += character - 'A' + 10;
            else fail("invalid Unicode escape");
        }
        return value;
    }

    std::string parse_string() {
        ++position_;
        std::string result;
        while (position_ < input_.size()) {
            const unsigned char character = static_cast<unsigned char>(input_[position_++]);
            if (character == '"') {
                return result;
            }
            if (character < 0x20) {
                fail("unescaped control character");
            }
            if (character != '\\') {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ == input_.size()) {
                fail("incomplete escape");
            }
            switch (input_[position_++]) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': append_utf8(result, parse_hex_quad()); break;
            default: fail("invalid escape");
            }
        }
        fail("unterminated string");
    }

    std::int64_t parse_integer() {
        const std::size_t start = position_;
        if (position_ < input_.size() && input_[position_] == '-') {
            ++position_;
        }
        while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
            ++position_;
        }
        std::int64_t result{};
        const auto parsed = std::from_chars(input_.data() + start, input_.data() + position_, result);
        if (parsed.ec != std::errc{} || parsed.ptr != input_.data() + position_) {
            fail("expected an integer");
        }
        return result;
    }

    void parse_literal(const std::string_view literal) {
        if (input_.substr(position_, literal.size()) != literal) {
            fail("invalid value");
        }
        position_ += literal.size();
    }

    std::string_view input_;
    std::size_t position_{};
};

const JsonValue& required(const JsonValue::Object& object, const std::string& key) {
    const auto found = object.find(key);
    if (found == object.end()) {
        throw std::runtime_error("Medication JSON is missing '" + key + "'");
    }
    return found->second;
}

template<typename T>
const T& as(const JsonValue& value, const char* field) {
    const T* result = std::get_if<T>(&value.value);
    if (!result) {
        throw std::runtime_error(std::string{"Medication JSON field '"} + field + "' has the wrong type");
    }
    return *result;
}

std::wstring from_utf8(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length == 0) throw std::runtime_error("Medication JSON contains invalid UTF-8");
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length == 0) throw std::runtime_error("Medication text contains invalid Unicode");
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::string format_timestamp(const std::chrono::system_clock::time_point value) {
    const auto seconds = std::chrono::floor<std::chrono::seconds>(value);
    const auto day = std::chrono::floor<std::chrono::days>(seconds);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{seconds - day};
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << static_cast<int>(date.year()) << '-'
           << std::setw(2) << static_cast<unsigned>(date.month()) << '-'
           << std::setw(2) << static_cast<unsigned>(date.day()) << 'T'
           << std::setw(2) << time.hours().count() << ':' << std::setw(2) << time.minutes().count() << ':'
           << std::setw(2) << time.seconds().count() << 'Z';
    return output.str();
}

int fixed_number(const std::string& value, const std::size_t offset, const std::size_t count) {
    int result{};
    const auto parsed = std::from_chars(value.data() + offset, value.data() + offset + count, result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + offset + count) {
        throw std::runtime_error("Medication timestamp is invalid");
    }
    return result;
}

std::chrono::system_clock::time_point parse_timestamp(const std::string& value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || value[19] != 'Z') {
        throw std::runtime_error("Medication timestamp must use UTC YYYY-MM-DDTHH:MM:SSZ format");
    }
    const std::chrono::year_month_day date{
        std::chrono::year{fixed_number(value, 0, 4)},
        std::chrono::month{static_cast<unsigned>(fixed_number(value, 5, 2))},
        std::chrono::day{static_cast<unsigned>(fixed_number(value, 8, 2))}};
    const int hour = fixed_number(value, 11, 2);
    const int minute = fixed_number(value, 14, 2);
    const int second = fixed_number(value, 17, 2);
    if (!date.ok() || hour > 23 || minute > 59 || second > 59) {
        throw std::runtime_error("Medication timestamp is out of range");
    }
    return std::chrono::sys_days{date} + std::chrono::hours{hour} + std::chrono::minutes{minute} +
           std::chrono::seconds{second};
}

std::string escape_json(const std::wstring& value) {
    const std::string utf8 = to_utf8(value);
    std::ostringstream output;
    for (const unsigned char character : utf8) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(character)
                       << std::dec;
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

Medication medication_from_json(const JsonValue& value) {
    const auto& object = as<JsonValue::Object>(value, "medication");
    const std::int64_t interval = as<std::int64_t>(required(object, "interval_minutes"), "interval_minutes");
    if (interval <= 0) {
        throw std::runtime_error("Medication interval_minutes must be positive");
    }

    Medication medication{
        .id = from_utf8(as<std::string>(required(object, "id"), "id")),
        .name = from_utf8(as<std::string>(required(object, "name"), "name")),
        .dose = from_utf8(as<std::string>(required(object, "dose"), "dose")),
        .interval = std::chrono::minutes{interval},
        .enabled = as<bool>(required(object, "enabled"), "enabled"),
    };
    if (const auto icon = object.find("icon_path"); icon != object.end() && !std::holds_alternative<std::nullptr_t>(icon->second.value)) {
        medication.icon_path = from_utf8(as<std::string>(icon->second, "icon_path"));
    }
    if (const auto taken = object.find("last_taken_at"); taken != object.end() && !std::holds_alternative<std::nullptr_t>(taken->second.value)) {
        medication.last_taken_at = parse_timestamp(as<std::string>(taken->second, "last_taken_at"));
    }
    return medication;
}

int setting_integer(const JsonValue::Object& object, const char* name) {
    const std::int64_t value = as<std::int64_t>(required(object, name), name);
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        throw std::runtime_error(std::string{"Medication JSON setting '"} + name + "' is out of range");
    }
    return static_cast<int>(value);
}

std::optional<int> optional_setting_integer(const JsonValue::Object& object, const char* name) {
    const JsonValue& value = required(object, name);
    if (std::holds_alternative<std::nullptr_t>(value.value)) return std::nullopt;
    return setting_integer(object, name);
}

BackgroundMaterial background_material_from_json(const JsonValue::Object& object) {
    const auto found = object.find("background_material");
    if (found == object.end()) return BackgroundMaterial::solid;
    const std::string& value = as<std::string>(found->second, "background_material");
    if (value == "solid") return BackgroundMaterial::solid;
    if (value == "mica") return BackgroundMaterial::mica;
    throw std::runtime_error("Medication JSON setting 'background_material' is invalid");
}
}

std::vector<Medication> load_medications(const std::filesystem::path& path, WidgetSettings* settings) {
    if (settings) *settings = {};
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (!std::filesystem::exists(path)) return {};
        throw std::runtime_error("Could not open medication JSON for reading");
    }
    const std::string contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const JsonValue document = JsonParser{contents}.parse();
    const auto& root = as<JsonValue::Object>(document, "root");
    if (settings) {
        if (const auto found = root.find("settings"); found != root.end()) {
            const auto& object = as<JsonValue::Object>(found->second, "settings");
            settings->window_x = optional_setting_integer(object, "window_x");
            settings->window_y = optional_setting_integer(object, "window_y");
            settings->position_locked = as<bool>(required(object, "position_locked"), "position_locked");
            settings->always_on_top = as<bool>(required(object, "always_on_top"), "always_on_top");
            settings->background_material = background_material_from_json(object);
        }
    }
    const auto& values = as<JsonValue::Array>(required(root, "medications"), "medications");
    std::vector<Medication> medications;
    medications.reserve(values.size());
    for (const JsonValue& value : values) medications.push_back(medication_from_json(value));
    return medications;
}

void save_medications(
    const std::filesystem::path& path, const std::span<const Medication> medications,
    const WidgetSettings& settings) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::filesystem::path temporary = path;
    temporary += L".tmp";

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Could not open temporary medication JSON for writing");
    output << "{\n  \"medications\": [";
    for (std::size_t index = 0; index < medications.size(); ++index) {
        const Medication& medication = medications[index];
        output << (index == 0 ? "\n" : ",\n")
               << "    {\n"
               << "      \"id\": \"" << escape_json(medication.id) << "\",\n"
               << "      \"name\": \"" << escape_json(medication.name) << "\",\n"
               << "      \"dose\": \"" << escape_json(medication.dose) << "\",\n"
               << "      \"icon_path\": ";
        if (medication.icon_path) output << '"' << escape_json(*medication.icon_path) << '"';
        else output << "null";
        output << ",\n      \"interval_minutes\": " << medication.interval.count() << ",\n"
               << "      \"last_taken_at\": ";
        if (medication.last_taken_at) output << '"' << format_timestamp(*medication.last_taken_at) << '"';
        else output << "null";
        output << ",\n      \"enabled\": " << (medication.enabled ? "true" : "false") << "\n    }";
    }
    output << (medications.empty() ? "]" : "\n  ]") << ",\n  \"settings\": {\n" << "    \"window_x\": ";
    if (settings.window_x) output << *settings.window_x;
    else output << "null";
    output << ",\n    \"window_y\": ";
    if (settings.window_y) output << *settings.window_y;
    else output << "null";
    output << ",\n    \"position_locked\": " << (settings.position_locked ? "true" : "false") << ",\n"
           << "    \"always_on_top\": " << (settings.always_on_top ? "true" : "false") << ",\n"
           << "    \"background_material\": \""
           << (settings.background_material == BackgroundMaterial::mica ? "mica" : "solid") << "\"\n"
           << "  }\n}\n";
    output.flush();
    if (!output) {
        output.close();
        std::filesystem::remove(temporary);
        throw std::runtime_error("Could not write medication JSON");
    }
    output.close();

    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        std::filesystem::remove(temporary);
        throw std::system_error(static_cast<int>(error), std::system_category(), "Could not replace medication JSON");
    }
}
