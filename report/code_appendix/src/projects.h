#ifndef PROJECTS_H
#define PROJECTS_H
#include "merge_plan.h"
// 正式排序返回的元数据和测量结果；不携带整个排序数组。
struct SortResult {
    int finalRun = -1; // 最终段目录下标；空输入保持-1
    uint64_t inputCount = 0; // 原始整数总数N
    size_t initialRuns = 0; // 生成阶段结束时的段数R
    uint64_t ordinaryCost = 0; // 普通二叉计划的理论加权长度
    uint64_t optimalCost = 0; // P2最优二叉计划的理论加权长度
    IoStats io; // 本次排序的数据块和阶段计数
    double milliseconds = 0; // sortFile内部steady_clock测得的毫秒数
};
// 统一外排序入口；创建runPath，按项目选择生成及归并策略。
SortResult sortFile(int project, const std::string& rawPath, const std::string& runPath,
                    size_t memorySize, size_t blockSize, size_t ways, std::ostream* trace = 0);
void printResult(const SortResult& result, std::ostream& output);
#endif
