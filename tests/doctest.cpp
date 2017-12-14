/*
 * Test Runner for Rack.
 * 
 * It help developing plugins faster, by providing
 * built-in test checks. And it can even 
 * render&display&execute your plugin
 * 
 * Run all tests, Rack Bultin and plugins.
 * $ ./doctest 
 * 
 * or only run tests on some plugin
 * $ ./doctest PluginName
 * 
 */
#include <stdio.h>

#define DOCTEST_CONFIG_IMPLEMENTATION_IN_DLL
#define DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "plugin.hpp"

using namespace rack;

int factorial(int number) { return number > 1 ? factorial(number - 1) * number : 1; }

TEST_SUITE("[doctest.cpp]") {
    TEST_CASE("testing the factorial function") {
        /*
        CHECK(factorial(1) == 1);
        CHECK(factorial(2) == 2);
        CHECK(factorial(3) == 6);
        CHECK(factorial(10) == 3628800);
        */
    }
}

int main(int argc, char** argv){
    
    std::string pluginName = "";
    if(argc > 1)
        pluginName = argv[1];

    /* Load plugins so tests are registered */
    if(pluginName == ""){
        pluginInit();
    } else {
	    loadPlugin("./plugins/"+pluginName);
    }

    doctest::Context context(argc, argv);
    int res = context.run();

    /* Destroy plugins */
    pluginDestroy();

    if(context.shouldExit())
        return res;

    return res;


}