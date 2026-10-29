#ifndef SERVER_HTTP_PARSE_UTILS_H
#define SERVER_HTTP_PARSE_UTILS_H

#include <cctype>
#include <string_view>

inline bool is_allowed_token_char(unsigned char c) {
    if (std::isalnum(c)) {
        return true;
    }
    static constexpr unsigned char allowed_characters[] = {
        '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.',
        '^', '_', '`', '|', '~'
    };
    for (unsigned char allowed : allowed_characters) {
        if (c == allowed) {
            return true;
        }
    }
    return false;
}

inline bool is_valid_token(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    for (unsigned char c : s) {
        if (!is_allowed_token_char(c)) {
            return false;
        }
    }
    return true;
}

inline bool has_forbidden_target_chars(std::string_view s) {
    // the target (path) should not contain any control characters or SPACE (0x20) or DEL (0x7F)
    for (unsigned char c : s) {
        if (c <= 0x20 || c == 0x7F) {
            return true;
        }
    }
    return false;
}

inline bool is_valid_field_value(std::string_view s) {
    // the header field value should not contain any control characters or TAB (\t) or DEL (0x7F)
    for (unsigned char c : s) {
        if ((c < 0x20 && c != '\t') || c == 0x7F) {
            return false;
        }
    }
    return true;
}

inline std::string_view trim_whitespace(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        start++;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        end--;
    }
    return s.substr(start, end - start);
}

#endif
