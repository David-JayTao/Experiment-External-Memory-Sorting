#ifndef MERGE_PLAN_H
#define MERGE_PLAN_H
#include "buffer.h"
#include <ostream>
// 二叉计划节点；保存依赖关系和权重，执行后缓存实际生成的段号。
struct MergeNode {
    int left = -1;                 // -1 表示叶子
    int right = -1; // 右孩子节点下标；叶子保持-1
    int runIndex = -1;             // 叶子对应原段；内部节点执行后记录结果
    uint64_t length = 0; // 子树代表的整数总数，建树时相加
};
// 以节点数组保存完整计划，供打印和后序执行复用。
struct MergePlan {
    std::vector<MergeNode> nodes;   // 孩子下标小于父亲，根在最后
    int root = -1;                 // 空输入没有根
    uint64_t cost = 0;             // 内部节点 length 之和，即加权外部路径长度
};
// 按M分块内排、按B写出；返回新建初始段目录下标。
std::vector<int> runGeneration(const std::string& rawPath, RunFile& file,
                               size_t memorySize, size_t blockSize, IoStats& stats);
// 两种建树都只读目录，返回节点数组；不在建树时执行数据归并。
MergePlan buildMergePlan(const RunFile& file, const std::vector<int>& runs);
MergePlan buildOptimalPlan(const RunFile& file, const std::vector<int>& runs);
// 以两个输入块和一个输出块归并，返回新增段号。
int twoWayMerge(RunFile& file, int leftRun, int rightRun, size_t blockSize, IoStats& stats);
// 后序执行指定子树；trace可选，仅用于演示。
int executeMergePlan(MergePlan& plan, int nodeIndex, RunFile& file,
                     size_t blockSize, IoStats& stats, std::ostream* trace = 0);
void printMergePlan(const MergePlan& plan, std::ostream& output);
#endif
