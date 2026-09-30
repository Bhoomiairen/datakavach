#pragma once
// A tiny streaming JSON writer (no dependencies). Enough for the engine's reports.
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace dk {

class JsonWriter {
public:
    explicit JsonWriter(std::string& out) : out_(out) {}

    JsonWriter& begin_object() { value_prefix(); out_ += '{'; stack_.push_back(true); return *this; }
    JsonWriter& end_object() { out_ += '}'; stack_.pop_back(); return *this; }
    JsonWriter& begin_array() { value_prefix(); out_ += '['; stack_.push_back(true); return *this; }
    JsonWriter& end_array() { out_ += ']'; stack_.pop_back(); return *this; }

    JsonWriter& key(std::string_view k) {
        comma();
        write_string(k);
        out_ += ':';
        after_key_ = true;
        return *this;
    }
    JsonWriter& value(std::string_view s) { value_prefix(); write_string(s); return *this; }
    JsonWriter& value(const char* s) { return value(std::string_view(s)); }
    JsonWriter& value(bool b) { value_prefix(); out_ += b ? "true" : "false"; return *this; }
    JsonWriter& value(long long n) { value_prefix(); out_ += std::to_string(n); return *this; }
    JsonWriter& value(unsigned long long n) { value_prefix(); out_ += std::to_string(n); return *this; }
    JsonWriter& value(int n) { return value(static_cast<long long>(n)); }
    JsonWriter& value(unsigned long n) { return value(static_cast<unsigned long long>(n)); }
    JsonWriter& value(double d) {
        value_prefix();
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.4g", d);
        out_ += buf;
        return *this;
    }

    template <typename T>
    JsonWriter& field(std::string_view k, const T& v) { key(k); return value(v); }

private:
    std::string& out_;
    std::vector<bool> stack_;  // true = no element written yet in this container
    bool after_key_ = false;

    void comma() {
        if (!stack_.empty()) {
            if (!stack_.back()) out_ += ',';
            stack_.back() = false;
        }
    }
    void value_prefix() {
        if (after_key_) { after_key_ = false; return; }
        comma();
    }
    void write_string(std::string_view s) {
        out_ += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': out_ += "\\\""; break;
                case '\\': out_ += "\\\\"; break;
                case '\n': out_ += "\\n"; break;
                case '\r': out_ += "\\r"; break;
                case '\t': out_ += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof buf, "\\u%04x", c);
                        out_ += buf;
                    } else {
                        out_ += static_cast<char>(c);
                    }
            }
        }
        out_ += '"';
    }
};

}  // namespace dk
