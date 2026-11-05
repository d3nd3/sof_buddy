#include "../src/features/list_match_fix/display_policy.h"

#include <cassert>
#include <cstring>

int main() {
    const char* first_label = "Deathmatch";
    const char* cvar_zero = "0";
    const char* displayed = list_match_fix::unmatched_value_should_be_shown_literally(cvar_zero)
                                ? cvar_zero
                                : first_label;
    assert(std::strcmp(displayed, "Deathmatch") == 0);

    assert(!list_match_fix::unmatched_value_should_be_shown_literally("0.0"));
    assert(!list_match_fix::unmatched_value_should_be_shown_literally("-0"));
    assert(list_match_fix::unmatched_value_should_be_shown_literally("923"));
    assert(list_match_fix::unmatched_value_should_be_shown_literally("custom"));
}
