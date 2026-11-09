#include "tests/test_sort.h"
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "week3";
        if (mode == "week3") week3Demo();
        else if (mode == "test") runTests();
        else throw std::runtime_error("usage: external_sort week3|test");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
