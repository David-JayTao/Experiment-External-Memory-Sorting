#include "projects.h"
#include "replacement.h"
#include "kway_merge.h"
#include <chrono>
#include <limits>
#include <stdexcept>

// 外排序正式入口。只在测试/演示层使用整份数据的 std::sort 参考结果。
// project选择1/2/3；rawPath为原始整数文件，runPath为新建段文件。
// memorySize为候选/内排容量，blockSize为块容量，ways为K路上限；返回指标和最终段号。
SortResult sortFile(int project, const std::string& rawPath, const std::string& runPath,
                    size_t memorySize, size_t blockSize, size_t ways, std::ostream* trace) {
    if (project < 1 || project > 3 || !memorySize || !blockSize || ways < 2 || rawPath == runPath)
        throw std::runtime_error("invalid sorting parameters");
    SortResult result;
    RawInput input(rawPath);
    result.inputCount = input.count();
    // 最坏初始段数不超过ceil(N/M)；二叉归并最多再增R-1段，预留足够目录。
    const uint64_t maximumRuns = input.count() / memorySize + (input.count() % memorySize != 0);
    if (maximumRuns > uint64_t((std::numeric_limits<int>::max() - 1) / 2))
        throw std::runtime_error("too many runs for this directory format");
    // 计时包括建段文件、生成及归并，排除调用方数据生成与正确性验证。
    const auto start = std::chrono::steady_clock::now();



    RunFile::create(runPath, static_cast<uint32_t>(2 * maximumRuns + 1));



    RunFile file(runPath);
    std::vector<int> runs;

    if (project == 1) 
        //选 Project 1，采用最普通的方法：每次读 M 个数，排序后写成一个归并段
        runs = runGeneration(rawPath, file, memorySize, blockSize, result.io);
    else 
        //选 Project 2 或 Project 3，采用置换选择算法，让初始归并段有机会变得更长
        runs = replacementSelection(rawPath, file, memorySize, blockSize, result.io, trace);
    
    //这两种方式最后都返回 runs。
    //runs 不是归并段里的全部整数，而是一组目录下标，用来告诉后续代码应该处理哪些归并段。

    result.initialRuns = runs.size();
    if (trace) {
        for (size_t index = 0; index < runs.size(); ++index)
        {
            const RunInfo& info = file.run(runs[index]);
            *trace << "initial " << info.name << " count=" << info.count << " offset=" << info.offset << '\n';
            // 只在显式 trace 中显示数据，不纳入排序 I/O 计数。
            if (input.count() <= 100) 
            {
                InputBuffer buffer(file, runs[index], blockSize);
                int32_t value;
                while (buffer.next(value)) *trace << value << ' ';
                *trace << '\n';
            }
        }
    }
    // 统一计算普通计划的理论cost；Project 3只比较该指标，不执行此二叉计划。
    MergePlan plan = buildMergePlan(file, runs);
    result.ordinaryCost = plan.cost;

    if (project == 2) 
    {
        plan = buildOptimalPlan(file, runs);
        result.optimalCost = plan.cost;
        if (trace) *trace << "fixed_chunk_runs=" << maximumRuns << " ordinary_cost=" << result.ordinaryCost
                          << " optimal_cost=" << result.optimalCost << '\n';
    }
    if (trace && project != 3) printMergePlan(plan, *trace);
    
    if (project == 3) 
        result.finalRun = mergeKRuns(file, runs, ways, blockSize, result.io, trace);
    else 
        result.finalRun = executeMergePlan(plan, plan.root, file, blockSize, result.io, trace);
    
        const auto end = std::chrono::steady_clock::now();
    result.milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}
// 汇总元素数、初始段、归并次数、轮数、逻辑数据块I/O、阶段数和毫秒。
void printResult(const SortResult& result, std::ostream& output) 
{
    output << "N=" << result.inputCount << " runs=" << result.initialRuns
           << " merges=" << result.io.merges << " rounds=" << result.io.rounds
           << " reads=" << result.io.readBlocks << " writes=" << result.io.writeBlocks
           << " stages=" << result.io.stages << " ms=" << result.milliseconds << '\n';
}
