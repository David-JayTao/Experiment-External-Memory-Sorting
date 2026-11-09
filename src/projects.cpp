#include "projects.h"
#include <chrono>
#include <limits>
#include <stdexcept>

// 外排序正式入口。只在测试/演示层使用整份数据的 std::sort 参考结果。
SortResult sortFile(int project, const std::string& rawPath, const std::string& runPath,
                    size_t memorySize, size_t blockSize, size_t ways, std::ostream* trace) {
    (void)ways;
    if (project != 1 || !memorySize || !blockSize || rawPath == runPath)
        throw std::runtime_error("invalid sorting parameters");
    SortResult result;
    RawInput input(rawPath);
    result.inputCount = input.count();
    const uint64_t maximumRuns = input.count() / memorySize + (input.count() % memorySize != 0);
    if (maximumRuns > (std::numeric_limits<uint32_t>::max() - 1) / 2)
        throw std::runtime_error("too many runs for this directory format");
    const auto start = std::chrono::steady_clock::now();
    RunFile::create(runPath, static_cast<uint32_t>(2 * maximumRuns + 1));
    RunFile file(runPath);
    std::vector<int> runs = runGeneration(rawPath, file, memorySize, blockSize, result.io);
    result.initialRuns = runs.size();
    if (trace) {
        for (size_t index = 0; index < runs.size(); ++index) {
            const RunInfo& info = file.run(runs[index]);
            *trace << "initial " << info.name << " count=" << info.count << " offset=" << info.offset << '\n';
            // 只在显式 trace 中显示数据，不纳入排序 I/O 计数。
            if (input.count() <= 100) {
                InputBuffer buffer(file, runs[index], blockSize);
                int32_t value;
                while (buffer.next(value)) *trace << value << ' ';
                *trace << '\n';
            }
        }
    }
    MergePlan plan = buildMergePlan(file, runs);
    result.ordinaryCost = plan.cost;
    if (trace) printMergePlan(plan, *trace);
    result.finalRun = executeMergePlan(plan, plan.root, file, blockSize, result.io, trace);
    const auto end = std::chrono::steady_clock::now();
    result.milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}
void printResult(const SortResult& result, std::ostream& output) {
    output << "N=" << result.inputCount << " runs=" << result.initialRuns
           << " merges=" << result.io.merges << " rounds=" << result.io.rounds
           << " reads=" << result.io.readBlocks << " writes=" << result.io.writeBlocks
           << " stages=" << result.io.stages << " ms=" << result.milliseconds << '\n';
}
