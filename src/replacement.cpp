#include "replacement.h"
#include "loser_tree.h"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <thread>

// 每个输出元素携带段号：一个块跨越段边界时，输出线程仍能正确更新目录。
struct GeneratedBlock 
{
    std::vector<int32_t> values;    // 当前块的有效输出，不超过 B 个
    std::vector<size_t> runNumbers; // 与 values 一一对应的生成段号
};
// 置换选择的单个物理输入块；两份交替承担计算输入和预取目标。
struct ReplacementInput 
{
    std::vector<int32_t> values;    // 固定 B 个整数的物理输入块
    size_t count = 0;              // 有效元素数量
    size_t pos = 0;                // 计算线程下一次消费的位置
};
// 按runNumbers把一个输出块切成同段连续片段。
// 跨段时先提交旧段再开启新段；runs记录已提交段，末段由主流程收尾。
static void writeGeneratedBlock(const GeneratedBlock& block, RunFile& file,
                                size_t& writtenRun, bool& runOpen,
                                std::vector<int>& runs, IoStats& stats) 
{
    size_t begin = 0;
    while (begin < block.values.size()) 
    {
        const size_t number = block.runNumbers[begin];
        if (!runOpen || number != writtenRun) 
        {
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
// 用M个候选执行置换选择，返回生成段号；B控制输入和输出块容量。
// 每阶段三个线程分别预取、选冠军、写上一块，join后才交接缓冲区。
std::vector<int> replacementSelection(const std::string& rawPath, RunFile& file,
                                      size_t memorySize, size_t blockSize, IoStats& stats,
                                      std::ostream* trace) 
{
    if (!memorySize || !blockSize) throw std::runtime_error("invalid replacement memory size");
    RawInput source(rawPath);
    ReplacementInput inputs[2];
    GeneratedBlock outputs[2];
    for (int index = 0; index < 2; ++index) 
    {
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
    // activeInput供计算消费，另一输入块供预取；输出也采用两份交替。
    int activeInput = 0, activeOutput = 0;
    size_t currentRun = 0, writtenRun = 0;
    bool runOpen = false;
    std::vector<int> runs;
    // 树空后还需把上一阶段生成的输出写完，不能立即退出。
    while (tree.winner() >= 0 || !outputs[1 - activeOutput].values.empty()) {


        const int spareInput = 1 - activeInput;
        const int writeOutput = 1 - activeOutput;

    
        const bool readNeeded = inputs[spareInput].count == 0 && loaded < source.count();
        // 阶段内只读取这个快照；不会读取输入线程正在更新的 count。
        const bool noMoreAfterActive = loaded == source.count() && inputs[spareInput].count == 0;
        outputs[activeOutput].values.clear();
        outputs[activeOutput].runNumbers.clear();
        // 每线程只写自己的异常槽；主线程join后统一重新抛出。
        std::exception_ptr errors[3];
        std::thread inputThread, computeThread, outputThread;
        try {
            // 输入线程只接触备用输入块和原始输入流，不改活动块。
            inputThread = std::thread([&]() {
                try {
                    if (readNeeded) inputs[spareInput].count = source.readBlock(inputs[spareInput].values.data(), blockSize, &stats);
                } catch (...) { errors[0] = std::current_exception(); }
            });
            // 计算线程独占败者树、活动输入位置和活动输出，不执行文件I/O。
            computeThread = std::thread([&]() {
                try {
                    ReplacementInput& input = inputs[activeInput];
                    GeneratedBlock& output = outputs[activeOutput];
                    while (output.values.size() < blockSize && tree.winner() >= 0) {
                        // 当前块耗尽但后面仍有数据时先停；不能把暂时缺数据当成EOF。
                        if (input.pos == input.count && !noMoreAfterActive) break;
                        // 最小有效候选也被冻结，说明当前段结束；全体解冻后进入下一段。
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

//冻结！//////////////////////////////////////////////////////////////////////////////////                            
                            // 新数小于刚输出值会破坏当前段非降序，因此留到下一段。
                            next.frozen = next.value < value;


                            if (trace && next.frozen) *trace << "freeze " << next.value << " < last=" << value << '\n';
                        }
                        tree.replaceWinner(next);
                        if (trace && source.count() <= 100) tree.print(*trace);
                    }
                } catch (...) { errors[1] = std::current_exception(); }
            });
            // 输出线程只写上一阶段的输出块，维护段文件和已提交段号。
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
        // 阶段屏障：所有读写完成后才能更新loaded、回收旧输出和切换角色。
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
