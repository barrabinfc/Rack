#pragma once

#include <string>

namespace rack {


void settingsSave(std::string filename);
void settingsLoad(std::string filename);

int factorial2(int number); //{ return number > 1 ? factorial2(number - 1) * number : 1; }

} // namespace rack

#define DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>

TEST_CASE("testing the factorial function") {
    CHECK(rack::factorial2(1) == 1);
    CHECK(rack::factorial2(2) == 2);
    CHECK(rack::factorial2(3) == 6);
    CHECK(rack::factorial2(10) == 3628800);
}
