#include "replacement.h"
#include "loser_tree.h"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <thread>

// 每个输出元素携带段号：一个块跨越段边界时，输出线程仍能正确更新目录。
struct GeneratedBlock {
    std::vector<int32_t> values;    // 当前块的有效输出，不超过 B 个
    std::vector<size_t> runNumbers; // 与 values 一一对应的生成段号
};
struct ReplacementInput {
    std::vector<int32_t> values;    // 固定 B 个整数的物理输入块
    size_t count = 0;              // 有效元素数量
    size_t pos = 0;                // 计算线程下一次消费的位置
};
static void writeGeneratedBlock(const GeneratedBlock& block, RunFile& file,
                                size_t& writtenRun, bool& runOpen,
                                std::vector<int>& runs, IoStats& stats) {
    size_t begin = 0;
    while (begin < block.values.size()) {
        const size_t number = block.runNumbers[begin];
        if (!runOpen || number != writtenRun) {
            if (runOpen) runs.push_back(file.endRun());
            file.beginRun("R" + std::to_string(number));
            writtenRun = number;
            runOpen = true;
        }
        size_t end = begin + 1;
        while (end < block.values.size() && block.runNumbers[end] == number) ++end;
        file.writeBlock(block.values.data() + begin, end - begin, &stats);
        begin = end;
    }
}
std::vector<int> replacementSelection(const std::string& rawPath, RunFile& file,
                                      size_t memorySize, size_t blockSize, IoStats& stats,
                                      std::ostream* trace) {
    if (!memorySize || !blockSize) throw std::runtime_error("invalid replacement memory size");
    RawInput source(rawPath);
    ReplacementInput inputs[2];
    GeneratedBlock outputs[2];
    for (int index = 0; index < 2; ++index) {
        inputs[index].values.resize(blockSize);
        outputs[index].values.reserve(blockSize);
        outputs[index].runNumbers.reserve(blockSize);
    }
    std::vector<Candidate> candidates(memorySize);
    uint64_t loaded = 0;
    size_t filled = 0;
    // 准备阶段把 M 个候选填入树，始终按不超过 B 的块读取。
    while (filled < memorySize) {
        size_t count = source.readBlock(inputs[0].values.data(), std::min(blockSize, memorySize - filled), &stats);
        if (!count) break;
        for (size_t index = 0; index < count; ++index) {
            candidates[filled].value = inputs[0].values[index];
            candidates[filled++].valid = true;
        }
        loaded += count;
    }
    LoserTree tree(candidates);
    std::vector<Candidate>().swap(candidates); // 初始化副本不再占用候选空间
    if (trace) tree.print(*trace);
    inputs[0].count = source.readBlock(inputs[0].values.data(), blockSize, &stats);
    loaded += inputs[0].count;
    int activeInput = 0, activeOutput = 0;
    size_t currentRun = 0, writtenRun = 0;
    bool runOpen = false;
    std::vector<int> runs;
    while (tree.winner() >= 0 || !outputs[1 - activeOutput].values.empty()) {
        const int spareInput = 1 - activeInput;
        const int writeOutput = 1 - activeOutput;
        const bool readNeeded = inputs[spareInput].count == 0 && loaded < source.count();
        // 阶段内只读取这个快照；不会读取输入线程正在更新的 count。
        const bool noMoreAfterActive = loaded == source.count() && inputs[spareInput].count == 0;
        outputs[activeOutput].values.clear();
        outputs[activeOutput].runNumbers.clear();
        std::exception_ptr errors[3];
        std::thread inputThread, computeThread, outputThread;
        try {
            inputThread = std::thread([&]() {
                try {
                    if (readNeeded) inputs[spareInput].count = source.readBlock(inputs[spareInput].values.data(), blockSize, &stats);
                } catch (...) { errors[0] = std::current_exception(); }
            });
            computeThread = std::thread([&]() {
                try {
                    ReplacementInput& input = inputs[activeInput];
                    GeneratedBlock& output = outputs[activeOutput];
                    while (output.values.size() < blockSize && tree.winner() >= 0) {
                        if (input.pos == input.count && !noMoreAfterActive) break;
                        if (tree.top().frozen) {
                            ++currentRun;
                            tree.thaw();
                            if (trace) *trace << "thaw -> run " << currentRun << '\n';
                        }
                        const int32_t value = tree.top().value;
                        output.values.push_back(value);
                        output.runNumbers.push_back(currentRun);
                        Candidate next;
                        if (input.pos < input.count) {
                            next.valid = true;
                            next.value = input.values[input.pos++];
                            next.frozen = next.value < value;
                            if (trace && next.frozen) *trace << "freeze " << next.value << " < last=" << value << '\n';
                        }
                        tree.replaceWinner(next);
                        if (trace && source.count() <= 100) tree.print(*trace);
                    }
                } catch (...) { errors[1] = std::current_exception(); }
            });
            outputThread = std::thread([&]() {
                try { writeGeneratedBlock(outputs[writeOutput], file, writtenRun, runOpen, runs, stats); }
                catch (...) { errors[2] = std::current_exception(); }
            });
        } catch (...) {
            // 若创建后续线程失败，先收回已启动线程，避免析构时 terminate。
            if (inputThread.joinable()) inputThread.join();
            if (computeThread.joinable()) computeThread.join();
            if (outputThread.joinable()) outputThread.join();
            throw;
        }
        inputThread.join(); computeThread.join(); outputThread.join();
        for (int index = 0; index < 3; ++index) if (errors[index]) std::rethrow_exception(errors[index]);
        ++stats.stages;
        if (readNeeded) loaded += inputs[spareInput].count;
        if (trace) *trace << "replacement sync " << stats.stages << " input=" << activeInput
                          << " consumed=" << inputs[activeInput].pos << '/' << inputs[activeInput].count
                          << " output=" << activeOutput << " count=" << outputs[activeOutput].values.size() << '\n';
        // 未用完的输入继续保留；只有真正耗尽才交换，避免段边界造成数据丢失。
        if (inputs[activeInput].pos == inputs[activeInput].count) {
            inputs[activeInput].pos = inputs[activeInput].count = 0;
            activeInput = spareInput;
        }
        outputs[writeOutput].values.clear();
        outputs[writeOutput].runNumbers.clear();
        activeOutput = writeOutput;
    }
    if (runOpen) runs.push_back(file.endRun());
    return runs;
}
