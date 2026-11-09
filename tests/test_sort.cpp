#include "test_sort.h"
#include "src/buffer.h"
#include "src/projects.h"
#include "src/loser_tree.h"
#include <algorithm>
#include <limits>
#include <random>
#include <iostream>
#include <stdexcept>
#include <cstdio>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
// 测试层允许加载参考数组；正式 sortFile 不接收整个输入数组。
static void checkSort(const std::vector<int32_t>& values, int project, size_t memorySize, size_t blockSize, size_t ways) {
    writeRaw("test_input.bin", values);
    SortResult result = sortFile(project, "test_input.bin", "test_runs.bin", memorySize, blockSize, ways);
    std::vector<int32_t> expected = values, actual;
    std::sort(expected.begin(), expected.end());
    RunFile file("test_runs.bin", false);
    if (result.finalRun >= 0) {
        InputBuffer buffer(file, result.finalRun, blockSize);
        int32_t value;
        while (buffer.next(value)) actual.push_back(value);
    }
    require(actual.size() == values.size(), "element count changed");
    require(std::is_sorted(actual.begin(), actual.end()), "output is not ordered");
    require(actual == expected, "output multiset differs from std::sort");
    // 每个中间归并段也必须有序。
    for (size_t index = 0; index < file.scan().size(); ++index) {
        InputBuffer buffer(file, static_cast<int>(index), blockSize);
        int32_t value = 0, previous = 0;
        bool first = true;
        while (buffer.next(value)) {
            require(first || previous <= value, "intermediate run is not ordered");
            previous = value;
            first = false;
        }
    }
}
void week3Demo() {
    RunFile::create("week3.bin", 8);
    {
        RunFile file("week3.bin");
        file.put("A", {-5, 2, 8});
        file.put("B", {0, 1});
        file.beginRun("C");
        OutputBuffer output(file, 2);
        for (int32_t value = 3; value < 8; ++value) output.writeValue(value);
        output.flush();
        file.endRun();
    }
    RunFile file("week3.bin");
    std::cout << "Header=" << sizeof(FileHeader) << " Directory=" << file.header().directoryOffset
              << " Data=" << file.header().dataOffset << '\n';
    for (size_t index = 0; index < file.scan().size(); ++index) {
        const RunInfo info = file.run(static_cast<int>(index));
        std::cout << "Scan " << info.name << " offset=" << info.offset << " count=" << info.count << ":";
        InputBuffer input(file, static_cast<int>(index), 2);
        int32_t value;
        while (input.next(value)) std::cout << ' ' << value;
        std::cout << '\n';
    }
    std::cout << "Search C=" << file.search("C") << " missing=" << file.search("missing") << '\n';
    require(file.search("C") == 2 && file.search("missing") == -1, "search failed");
}
void runTests() {
    week3Demo();
    RunFile::create("test_file.bin", 2);
    {
        RunFile file("test_file.bin");
        file.put("empty", {});
        file.beginRun("tail");
        IoStats stats;
        OutputBuffer output(file, 3, &stats);
        for (int32_t value = -5; value < 5; ++value) output.writeValue(value);
        output.flush();
        file.endRun();
        require(stats.writeBlocks == 4, "write block count");
        bool rejected = false;
        try { file.beginRun("full"); } catch (const std::exception&) { rejected = true; }
        require(rejected, "capacity check");
    }
    {
        RunFile file("test_file.bin", false);
        InputBuffer empty(file, 0, 3);
        int32_t value;
        require(!empty.next(value), "empty run");
        IoStats stats;
        InputBuffer input(file, 1, 3, &stats);
        for (int32_t expected = -5; expected < 5; ++expected)
            require(input.next(value) && value == expected, "buffer round trip");
        require(!input.next(value) && stats.readBlocks == 4, "tail or count");
    }
    std::remove("test_file.bin");
    std::cout << "PASS week3 file/buffer tests\n";
    std::vector<std::vector<int32_t> > cases = {{}, {7}, {3, 2, 1}, {-1, -1, -1, 0, -2},
        {std::numeric_limits<int32_t>::min(), 0, std::numeric_limits<int32_t>::max()}};
    std::mt19937 random(20261109);
    for (size_t count : {size_t(17), size_t(257), size_t(4099)}) {
        std::vector<int32_t> values(count);
        for (size_t index = 0; index < count; ++index) values[index] = int32_t(random() % 101) - 50;
        cases.push_back(values);
        std::sort(values.begin(), values.end());
        cases.push_back(values);
        std::reverse(values.begin(), values.end());
        cases.push_back(values);
    }
    for (size_t index = 0; index < cases.size(); ++index) {
        checkSort(cases[index], 1, 23, 7, 3);
        checkSort(cases[index], 2, 23, 7, 3);
    }
    for (size_t memorySize : {size_t(1), size_t(3), size_t(8)})
        for (size_t blockSize : {size_t(1), size_t(5), size_t(17)})
            checkSort({9, 1, 8, 2, 7, 3, 6, 4, 5, 0, -1, 3, 12, 2, 10}, 2, memorySize, blockSize, 3);
    std::cout << "PASS project1 " << cases.size() << " cases against std::sort\n";
}
