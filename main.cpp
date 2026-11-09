#include "tests/test_sort.h"
#include "src/projects.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
// 严格解析非负整数，避免负数被转换成巨大的无符号容量。
static size_t number(const char* text) {
    if (!text[0]) throw std::runtime_error("empty numeric option");
    for (const char* digit = text; *digit; ++digit)
        if (*digit < '0' || *digit > '9') throw std::runtime_error("numeric option must contain digits only");
    errno = 0;
    char* end = 0;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno || *end || value > std::numeric_limits<size_t>::max()) throw std::runtime_error("numeric option out of range");
    return static_cast<size_t>(value);
}
static void usage() {
    std::cout << "external_sort week3 | test | benchmark\n"
              << "external_sort project1|project2|project3 [--memory M] [--block B] [--k K] [--trace]\n"
              << "  [--size N | --input raw.bin] [--output runs.bin]\n"
              << "No size/input: 15-value traced demo with exact std::sort verification.\n"
              << "--size: fixed-seed generated data with exact verification.\n"
              << "--input: raw native-endian int32 file; streaming count/order verification.\n"
              << "Output is Header+Directory+Data. M/B are element counts, K >= 2.\n";
}
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "help";
        if (mode == "help" || mode == "--help") { usage(); return 0; }
        if (mode == "week3" || mode == "test" || mode == "benchmark") {
            if (argc != 2) throw std::runtime_error("this mode accepts no options");
            if (mode == "week3") week3Demo();
            else if (mode == "test") runTests();
            else runBenchmarks();
            return 0;
        }
        if (mode != "project1" && mode != "project2" && mode != "project3")
            throw std::runtime_error("unknown mode; run external_sort --help");
        size_t memorySize = 4, blockSize = 3, ways = 3, count = 15;
        bool generatedSize = false, trace = false;
        std::string inputPath, outputPath = mode + "_runs.bin";
        for (int index = 2; index < argc; ++index) {
            const std::string option = argv[index];
            if (option == "--trace") { trace = true; continue; }
            if (++index == argc) throw std::runtime_error("missing option value");
            if (option == "--memory") memorySize = number(argv[index]);
            else if (option == "--block") blockSize = number(argv[index]);
            else if (option == "--k") ways = number(argv[index]);
            else if (option == "--size") { count = number(argv[index]); generatedSize = true; }
            else if (option == "--input") inputPath = argv[index];
            else if (option == "--output") outputPath = argv[index];
            else throw std::runtime_error("unknown option " + option);
        }
        if (generatedSize && !inputPath.empty()) throw std::runtime_error("choose --size or --input");
        if (!memorySize || !blockSize || ways < 2) throw std::runtime_error("M/B must be positive and K >= 2");
        const bool exactCheck = inputPath.empty();
        std::vector<int32_t> expected;
        if (exactCheck) {
            inputPath = mode + "_input.bin";
            if (generatedSize) {
                std::mt19937 random(20261109);
                expected.resize(count);
                for (size_t index = 0; index < count; ++index) expected[index] = int32_t(random() % 20001) - 10000;
            } else {
                expected = {9, 1, 8, 2, 7, 3, 6, 4, 5, 0, -1, 3, 12, 2, 10};
                trace = true;
            }
            if (outputPath == inputPath) throw std::runtime_error("input and output must differ");
            writeRaw(inputPath, expected);
            if (trace && expected.size() <= 100) {
                std::cout << "input:";
                for (int32_t value : expected) std::cout << ' ' << value;
                std::cout << '\n';
            }
        }
        SortResult result = sortFile(mode[7] - '0', inputPath, outputPath, memorySize, blockSize, ways, trace ? &std::cout : 0);
        if (exactCheck) std::sort(expected.begin(), expected.end());
        RunFile file(outputPath, false);
        uint64_t checked = 0;
        int32_t previous = 0;
        if (result.finalRun >= 0) {
            InputBuffer input(file, result.finalRun, blockSize);
            int32_t value;
            if (trace && result.inputCount <= 100) std::cout << "output:";
            while (input.next(value)) {
                if (checked && value < previous) throw std::runtime_error("output not ordered");
                if (exactCheck && (checked >= expected.size() || expected[size_t(checked)] != value))
                    throw std::runtime_error("std::sort exact comparison failed");
                if (trace && result.inputCount <= 100) std::cout << ' ' << value;
                previous = value;
                ++checked;
            }
            if (trace && result.inputCount <= 100) std::cout << '\n';
        }
        if (checked != result.inputCount) throw std::runtime_error("output count changed");
        std::cout << (exactCheck ? "PASS std::sort exact comparison\n" : "PASS streaming count/order check (not an exact multiset comparison)\n");
        printResult(result, std::cout);
        std::cout << "result_file=" << outputPath << " final_run=" << result.finalRun;
        if (result.finalRun >= 0) std::cout << " name=" << file.run(result.finalRun).name;
        std::cout << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
