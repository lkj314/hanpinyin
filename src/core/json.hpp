// HanPinyin · 极简 JSON 解析 / 序列化（header-only，仅所需子集）
//
// 支持：对象 / 数组 / 字符串（含转义与 \uXXXX）/ 数字 / 布尔 / null。
// 字符串内部以 UTF-8 的 std::string 保存（韩文即原始 UTF-8 字节），
// 调用方用 types.h 的 utf8_to_wstring / wstring_to_utf8 与 std::wstring 互转。
//
// 本文件属于 core，严禁 #include <windows.h>，仅依赖标准库。

#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace hp_json {

// JSON 值容器（动态类型）
class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj;  // 保序的对象成员

    Value() = default;
    Value(bool b) : type(Type::Bool), boolVal(b) {}
    Value(int n) : type(Type::Number), numVal(static_cast<double>(n)) {}
    Value(double n) : type(Type::Number), numVal(n) {}
    Value(const std::string& s) : type(Type::String), strVal(s) {}
    Value(const char* s) : type(Type::String), strVal(s ? s : "") {}
    Value(Type t) : type(t) {}  // 按指定类型构造空容器（Array / Object）

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }
    bool isBool() const { return type == Type::Bool; }
    bool isNull() const { return type == Type::Null; }

    // 对象取值（const 与 mutable 两个版本）
    const Value& operator[](const std::string& k) const {
        static const Value kNull;
        for (const auto& p : obj) {
            if (p.first == k) return p.second;
        }
        return kNull;
    }
    Value& operator[](const std::string& k) {
        for (auto& p : obj) {
            if (p.first == k) return p.second;
        }
        obj.emplace_back(k, Value());
        return obj.back().second;
    }
    // 数组按索引取值
    const Value& operator[](size_t i) const { return arr[i]; }

    bool has(const std::string& k) const {
        for (const auto& p : obj) {
            if (p.first == k) return true;
        }
        return false;
    }
    size_t size() const { return arr.size(); }

    // 便捷取值
    int asInt() const { return static_cast<int>(numVal); }
    double asDouble() const { return numVal; }
    const std::string& asString() const { return strVal; }
    bool asBool() const { return boolVal; }
};

// 递归下降解析器
class Parser {
public:
    explicit Parser(const std::string& src) : s_(src), i_(0) {}

    Value parse() {
        skipWs();
        Value v = parseValue();
        skipWs();
        return v;
    }

private:
    const std::string& s_;
    size_t i_;

    void skipWs() {
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i_;
            } else {
                break;
            }
        }
    }

    Value parseValue() {
        skipWs();
        if (i_ >= s_.size()) return Value();
        char c = s_[i_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') {
            Value v;
            v.type = Value::Type::String;
            v.strVal = parseString();
            return v;
        }
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') {
            i_ += 4;  // null
            return Value();
        }
        return parseNumber();
    }

    Value parseObject() {
        Value v;
        v.type = Value::Type::Object;
        ++i_;  // 跳过 {
        skipWs();
        if (i_ < s_.size() && s_[i_] == '}') { ++i_; return v; }
        while (i_ < s_.size()) {
            skipWs();
            if (s_[i_] != '"') break;
            std::string key = parseString();
            skipWs();
            if (i_ < s_.size() && s_[i_] == ':') ++i_;
            skipWs();
            Value val = parseValue();
            v.obj.emplace_back(std::move(key), std::move(val));
            skipWs();
            if (i_ < s_.size() && s_[i_] == ',') { ++i_; continue; }
            if (i_ < s_.size() && s_[i_] == '}') { ++i_; break; }
        }
        return v;
    }

    Value parseArray() {
        Value v;
        v.type = Value::Type::Array;
        ++i_;  // 跳过 [
        skipWs();
        if (i_ < s_.size() && s_[i_] == ']') { ++i_; return v; }
        while (i_ < s_.size()) {
            skipWs();
            if (s_[i_] == ']') { ++i_; break; }
            Value val = parseValue();
            v.arr.push_back(std::move(val));
            skipWs();
            if (i_ < s_.size() && s_[i_] == ',') { ++i_; continue; }
            if (i_ < s_.size() && s_[i_] == ']') { ++i_; break; }
        }
        return v;
    }

    std::string parseString() {
        std::string out;
        ++i_;  // 跳过开头的 "
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (c == '"') { ++i_; break; }
            if (c == '\\') {
                ++i_;
                if (i_ >= s_.size()) break;
                char e = s_[i_];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned int cp = parseHex4();
                        // 处理代理对
                        if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 2 < s_.size() &&
                            s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                            i_ += 2;
                            unsigned int lo = parseHex4();
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: out.push_back(e); break;
                }
                ++i_;
            } else {
                out.push_back(c);
                ++i_;
            }
        }
        return out;
    }

    unsigned int parseHex4() {
        unsigned int v = 0;
        for (int k = 0; k < 4 && i_ < s_.size(); ++k, ++i_) {
            char c = s_[i_];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (c - '0');
            else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
        }
        return v;
    }

    static void appendUtf8(std::string& out, unsigned int cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    Value parseNumber() {
        size_t start = i_;
        while (i_ < s_.size()) {
            char c = s_[i_];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
                c == 'e' || c == 'E') {
                ++i_;
            } else {
                break;
            }
        }
        Value v;
        v.type = Value::Type::Number;
        v.numVal = std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
        return v;
    }

    Value parseBool() {
        if (i_ < s_.size() && s_[i_] == 't') {
            i_ += 4;  // true
            return Value(true);
        }
        i_ += 5;  // false
        return Value(false);
    }
};

// 序列化器
class Writer {
public:
    explicit Writer(int indent = 0) : indent_(indent), level_(0) {}

    std::string stringify(const Value& v) {
        std::string out;
        writeValue(v, out);
        return out;
    }

private:
    int indent_;
    int level_;

    void newline(std::string& out) {
        if (indent_ <= 0) return;
        out.push_back('\n');
        out.append(static_cast<size_t>(indent_) * static_cast<size_t>(level_), ' ');
    }

    void writeValue(const Value& v, std::string& out) {
        switch (v.type) {
            case Value::Type::Null: out += "null"; break;
            case Value::Type::Bool: out += v.boolVal ? "true" : "false"; break;
            case Value::Type::Number: {
                char buf[64];
                // 整数外形直接输出，避免 1.0 之类
                if (v.numVal == static_cast<double>(static_cast<long long>(v.numVal))) {
                    snprintf(buf, sizeof(buf), "%lld",
                             static_cast<long long>(v.numVal));
                } else {
                    snprintf(buf, sizeof(buf), "%.6g", v.numVal);
                }
                out += buf;
                break;
            }
            case Value::Type::String: writeString(v.strVal, out); break;
            case Value::Type::Array: writeArray(v, out); break;
            case Value::Type::Object: writeObject(v, out); break;
        }
    }

    void writeString(const std::string& s, std::string& out) {
        out.push_back('"');
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x",
                                 static_cast<unsigned int>(static_cast<unsigned char>(c)));
                        out += buf;
                    } else {
                        out.push_back(c);
                    }
            }
        }
        out.push_back('"');
    }

    void writeArray(const Value& v, std::string& out) {
        if (v.arr.empty()) { out += "[]"; return; }
        out.push_back('[');
        ++level_;
        for (size_t k = 0; k < v.arr.size(); ++k) {
            if (k > 0) out.push_back(',');
            newline(out);
            writeValue(v.arr[k], out);
        }
        --level_;
        newline(out);
        out.push_back(']');
    }

    void writeObject(const Value& v, std::string& out) {
        if (v.obj.empty()) { out += "{}"; return; }
        out.push_back('{');
        ++level_;
        for (size_t k = 0; k < v.obj.size(); ++k) {
            if (k > 0) out.push_back(',');
            newline(out);
            writeString(v.obj[k].first, out);
            out.push_back(':');
            if (indent_ > 0) out.push_back(' ');
            writeValue(v.obj[k].second, out);
        }
        --level_;
        newline(out);
        out.push_back('}');
    }
};

// 解析入口
inline Value parse(const std::string& text) {
    Parser p(text);
    return p.parse();
}

// 序列化入口（indent<=0 表示紧凑输出）
inline std::string stringify(const Value& v, int indent = 2) {
    Writer w(indent);
    return w.stringify(v);
}

}  // namespace hp_json
