#include "test_sort.h"
#include "src/buffer.h"
#include <iostream>
#include <stdexcept>
#include <cstdio>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
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
}
