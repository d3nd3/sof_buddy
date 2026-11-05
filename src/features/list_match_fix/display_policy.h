#pragma once

#include <cctype>
#include <cstdlib>

namespace list_match_fix {

inline bool unmatched_value_should_be_shown_literally(const char* value) {
    if (!value) return true;
    char* end = nullptr;
    const double numeric = std::strtod(value, &end);
    if (end == value || numeric != 0.0) return true;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return *end != '\0';
}

} // namespace list_match_fix
