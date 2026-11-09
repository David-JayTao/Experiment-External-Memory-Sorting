#include "test_sort.h"
#include "src/buffer.h"
#include "src/projects.h"
#include "src/loser_tree.h"
#include "src/kway_merge.h"
#include <algorithm>
#include <limits>
#include <random>
#include <iostream>
#include <stdexcept>
#include <cstdio>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void testBufferPool() {
    RunFile::create("test_pool.bin", 4);
    RunFile file("test_pool.bin");
    std::vector<int> runs;
    runs.push_back(file.put("low", {0, 1, 2, 3}));
    runs.push_back(file.put("middle", {100, 101}));
    runs.push_back(file.put("high", {200, 201}));
    BufferPool pool(3, 1, file.scan());
    IoStats stats;
    pool.initialization(file, runs, stats);
    require(pool.blockCount() == 6 && pool.freeCount() == 3, "2k input pool initialization");
    for (int repeat = 0; repeat < 3; ++repeat) {
        require(pool.choosePrefetch() == 0, "smallest last-key policy");
        int block = pool.reserve();
        require(block >= 0, "prefetch allocation");
        pool.readPrefetch(file, runs[0], 0, block, stats);
        pool.finishPrefetch(0, block);
    }
    // lane0 此时拥有 4 个块，证明没有永久固定每路两个。
    require(pool.freeCount() == 0, "shared pool use");
    for (int expected = 0; expected < 4; ++expected) {
        require(pool.front(0) == expected, "queued block ordering");
        pool.consume(0);
    }
    pool.releaseConsumed();
    require(pool.freeCount() == 4 && !pool.needsData(0), "release all consumed blocks");
    require(pool.choosePrefetch() == 1, "finished run must not be prefetched");
    const int recycled = pool.reserve();
    require(recycled == 0, "released block should be reusable by another lane");
    pool.readPrefetch(file, runs[1], 1, recycled, stats);
    pool.finishPrefetch(1, recycled);
    pool.consume(1);
    require(pool.front(1) == 101, "cross-lane buffer reuse");
    std::cout << "PASS dynamic pool: 2k blocks, argmin last-key, four blocks in one lane, cross-lane reuse\n";
}
// 测试层允许加载参考数组；正式 sortFile 不接收整个输入数组。
static SortResult checkSort(const std::vector<int32_t>& values, int project, size_t memorySize, size_t blockSize, size_t ways) {
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
    return result;
}
static void testMechanisms() {
    std::vector<Candidate> leaves(5);
    const int32_t values[] = {4, 3, 6, 8, 1};
    for (size_t index = 0; index < leaves.size(); ++index) { leaves[index].value = values[index]; leaves[index].valid = true; }
    LoserTree tree(leaves);
    require(tree.winner() == 4 && tree.top().value == 1, "loser-tree initial champion");
    Candidate next;
    next.valid = true; next.value = 0; next.frozen = true;
    tree.replaceWinner(next);
    require(tree.top().value == 3, "frozen candidate participated in current run");
    for (int32_t expected : {3, 4, 6, 8}) {
        require(tree.top().value == expected, "loser replay path");
        tree.replaceWinner(Candidate());
    }
    require(tree.top().frozen && tree.top().value == 0, "frozen data was lost");
    tree.thaw();
    require(!tree.top().frozen && tree.top().value == 0, "thaw failed");
    tree.replaceWinner(Candidate());
    require(tree.winner() == -1, "tree EOF failed");

    RunFile::create("test_plan.bin", 16);
    RunFile file("test_plan.bin");
    std::vector<int> runs;
    for (int length : {1, 2, 3, 100}) runs.push_back(file.put("R" + std::to_string(length), std::vector<int32_t>(length, length)));
    require(buildMergePlan(file, runs).cost == 212, "ordinary plan cost");
    require(buildOptimalPlan(file, runs).cost == 115, "Huffman weighted path cost");
    const int empty = file.put("empty", {});
    IoStats stats;
    int result = kWayMerge(file, {empty, runs[0], empty}, 4, 1, stats);
    require(file.run(result).count == 1, "k-way empty lanes");
    result = kWayMerge(file, {empty, empty}, 4, 3, stats);
    require(file.run(result).count == 0, "all empty merge lanes");
    bool rejected = false;
    try { int32_t value; file.readBlock(runs[0], 2, &value, 1); }
    catch (const std::exception&) { rejected = true; }
    require(rejected, "read beyond run boundary accepted");
    rejected = false;
    try { file.beginRun("R1"); } catch (const std::exception&) { rejected = true; }
    require(rejected, "duplicate name accepted");
    rejected = false;
    try { InputBuffer bad(file, -1, 1); } catch (const std::exception&) { rejected = true; }
    require(rejected, "missing run accepted");
    {
        std::ofstream bad("test_bad.bin", std::ios::binary);
        bad << 'x';
    }
    rejected = false;
    try { RawInput bad("test_bad.bin"); } catch (const std::exception&) { rejected = true; }
    require(rejected, "partial int32 accepted");
    rejected = false;
    try { RunFile bad("test_bad.bin"); } catch (const std::exception&) { rejected = true; }
    require(rejected, "truncated header accepted");
    std::cout << "PASS loser replay/freeze/EOF, Huffman cost, empty K lanes, invalid files/ranges/names\n";
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
    testMechanisms();
    testBufferPool();
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
        for (size_t ways : {size_t(2), size_t(3), size_t(4)})
            checkSort(cases[index], 3, 23, 7, ways);
    }
    for (size_t memorySize : {size_t(1), size_t(3), size_t(8)})
        for (size_t blockSize : {size_t(1), size_t(5), size_t(17)})
            for (int project = 1; project <= 3; ++project)
                checkSort({9, 1, 8, 2, 7, 3, 6, 4, 5, 0, -1, 3, 12, 2, 10}, project, memorySize, blockSize, 3);
    std::cout << "PASS project1 " << cases.size() << " cases against std::sort\n";
    std::cout << "PASS project2/project3 with k=2,3,4 and memory/block boundary combinations\n";
    for (unsigned seed = 0; seed < 40; ++seed) {
        std::mt19937 randomCase(seed);
        std::vector<int32_t> values(randomCase() % 100);
        for (size_t index = 0; index < values.size(); ++index) values[index] = int32_t(randomCase() % 11) - 5;
        const size_t memorySize = randomCase() % 9 + 1, blockSize = randomCase() % 11 + 1, ways = randomCase() % 6 + 2;
        for (int project = 1; project <= 3; ++project) checkSort(values, project, memorySize, blockSize, ways);
    }
    std::vector<int32_t> large(50003);
    for (size_t index = 0; index < large.size(); ++index) large[index] = int32_t(random() % 20001) - 10000;
    for (int project = 1; project <= 3; ++project) checkSort(large, project, 256, 64, 4);
    std::cout << "PASS 220 end-to-end exact sorting checks, including 50003 elements\n";
}

// CSV 时间只覆盖 sortFile；数据生成、std::sort、逐个中间段检查不计时。
void runBenchmarks() {
    std::cout << "project,n,memory,block,k,runs,merges,rounds,reads,writes,stages,ms,exact\n";
    for (size_t count : {size_t(31), size_t(257), size_t(4099), size_t(50003)}) {
        std::mt19937 random(20261109);
        std::vector<int32_t> values(count);
        for (size_t index = 0; index < count; ++index) values[index] = int32_t(random() % 20001) - 10000;
        for (int project = 1; project <= 3; ++project) {
            SortResult result = checkSort(values, project, 256, 64, 4);
            std::cout << project << ',' << count << ",256,64,4," << result.initialRuns << ','
                      << result.io.merges << ',' << result.io.rounds << ',' << result.io.readBlocks << ','
                      << result.io.writeBlocks << ',' << result.io.stages << ',' << result.milliseconds << ",PASS\n";
        }
    }
}
