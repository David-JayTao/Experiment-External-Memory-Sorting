#include "tests/test_sort.h"
#include "src/projects.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "week3";
        if (mode == "week3") week3Demo();
        else if (mode == "test") runTests();
        else if (mode == "project1" || mode == "project2" || mode == "project3") {
            const std::vector<int32_t> values = {9, 1, 8, 2, 7, 3, 6, 4, 5, 0, -1, 3, 12, 2, 10};
            writeRaw("demo_input.bin", values);
            std::cout << "input:";
            for (int32_t value : values) std::cout << ' ' << value;
            std::cout << '\n';
            SortResult result = sortFile(mode[7] - '0', "demo_input.bin", "demo_runs.bin", 4, 3, 3, &std::cout);
            RunFile file("demo_runs.bin", false);
            InputBuffer input(file, result.finalRun, 3);
            std::vector<int32_t> actual, expected = values;
            int32_t value;
            std::cout << "output:";
            while (input.next(value)) { actual.push_back(value); std::cout << ' ' << value; }
            std::sort(expected.begin(), expected.end());
            if (actual != expected) throw std::runtime_error("std::sort verification failed");
            std::cout << "\nPASS std::sort exact comparison\n";
            printResult(result, std::cout);
        }
        else throw std::runtime_error("usage: external_sort week3|test");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
