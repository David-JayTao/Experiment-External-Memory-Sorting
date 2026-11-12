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
#ifdef _WIN32
#include <direct.h>
#endif
// 严格解析非负整数，避免负数被转换成巨大的无符号容量。
static size_t number(const char* text) 
{
    if (!text[0]) throw std::runtime_error("empty numeric option");
    for (const char* digit = text; *digit; ++digit)
        if (*digit < '0' || *digit > '9') throw std::runtime_error("numeric option must contain digits only");
    errno = 0;
    char* end = 0;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno || *end || value > std::numeric_limits<size_t>::max()) throw std::runtime_error("numeric option out of range");
    return static_cast<size_t>(value);
}
// 显示统一命令行入口及输入格式；帮助模式不创建数据文件。
static void usage() {
#ifdef _WIN32
    std::cout << "Windows optional prefix: external_sort --workdir DIRECTORY MODE [options]\n";
#endif
    std::cout << "external_sort week3 | test | benchmark\n"
              << "external_sort project1|project2|project3 [--memory M] [--block B] [--k K] [--trace]\n"
              << "  [--size N | --input raw.bin] [--output runs.bin]\n"
              << "No size/input: 15-value traced demo with exact std::sort verification.\n"
              << "--size: fixed-seed generated data with exact verification.\n"
              << "--input: raw native-endian int32 file; streaming count/order verification.\n"
              << "Output is Header+Directory+Data. M/B are element counts, K >= 2.\n";
}
// 解析模式和容量参数，调用演示、测试或 sortFile。
// 正常退出返回0；参数、文件或验证错误统一报告并返回1。
int main(int argc, char** argv) 
{
    try {
#ifdef _WIN32
        // C/C++ 1.35 的 Ctrl+F5 复用终端时不更新 cwd/环境变量。
        // Windows 验收配置显式传此前缀，保证数据位置；普通命令不受影响。
        if (argc > 1 && std::string(argv[1]) == "--workdir") {
            if (argc < 4) throw std::runtime_error("--workdir needs a directory and mode");
            if (_chdir(argv[2]) != 0) throw std::runtime_error("cannot enter --workdir directory");
            argc -= 2;
            argv += 2; // 跳过目录前缀，让原有模式解析继续使用 argv[1]。
        }
#endif
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
        // M、B以整数个数计；默认15个数用于验收逐步观察。
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
        // 自动生成数据可保留参考数组；自定义文件仅做流式数量和有序性验证。
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
        // 正式算法只接收文件路径；生成参考数据与最终验证均在算法计时之外。
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
        // 数量相同且逐项等于std::sort，才能证明未丢失或重复数据。
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
