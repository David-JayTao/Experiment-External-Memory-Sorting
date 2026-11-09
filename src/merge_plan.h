#ifndef MERGE_PLAN_H
#define MERGE_PLAN_H
#include "buffer.h"
#include <ostream>
struct MergeNode {
    int left = -1;                 // -1 表示叶子
    int right = -1;
    int runIndex = -1;             // 叶子对应原段；内部节点执行后记录结果
    uint64_t length = 0;
};
struct MergePlan {
    std::vector<MergeNode> nodes;   // 孩子下标小于父亲，根在最后
    int root = -1;                 // 空输入没有根
    uint64_t cost = 0;             // 内部节点 length 之和，即加权外部路径长度
};
std::vector<int> runGeneration(const std::string& rawPath, RunFile& file,
                               size_t memorySize, size_t blockSize, IoStats& stats);
// 两种建树都只读目录，返回节点数组；不在建树时执行数据归并。
MergePlan buildMergePlan(const RunFile& file, const std::vector<int>& runs);
MergePlan buildOptimalPlan(const RunFile& file, const std::vector<int>& runs);
int twoWayMerge(RunFile& file, int leftRun, int rightRun, size_t blockSize, IoStats& stats);
int executeMergePlan(MergePlan& plan, int nodeIndex, RunFile& file,
                     size_t blockSize, IoStats& stats, std::ostream* trace = 0);
void printMergePlan(const MergePlan& plan, std::ostream& output);
#endif
