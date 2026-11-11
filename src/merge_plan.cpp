#include "merge_plan.h"
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <functional>

// memorySize 是本次内排的元素上限；输出继续复用 Week 3 缓冲区。
// 从rawPath每次取至多M个整数内排，按B输出；返回初始段目录下标序列。
std::vector<int> runGeneration(const std::string& rawPath, RunFile& file,
                               size_t memorySize, size_t blockSize, IoStats& stats) {
    if (!memorySize || !blockSize) throw std::runtime_error("memory and block sizes must be positive");
    RawInput input(rawPath);
    std::vector<int32_t> values(memorySize);
    std::vector<int> runs;
    while (true) {
        // 读取调用也受 blockSize 限制，便于同一块大小下比较 I/O。
        size_t count = 0;
        while (count < memorySize) {
            size_t read = input.readBlock(values.data() + count, std::min(blockSize, memorySize - count), &stats);
            if (!read) break;
            count += read;
        }
        if (!count) break;
        std::sort(values.begin(), values.begin() + count);
        file.beginRun("R" + std::to_string(runs.size()));
        OutputBuffer output(file, blockSize, &stats);
        for (size_t index = 0; index < count; ++index) output.writeValue(values[index]);
        output.flush();
        runs.push_back(file.endRun());
    }
    return runs;
}
// 普通计划：每轮相邻配对；奇数个节点时最后一个直接进入下一轮。
// 输入段号对应计划叶子；返回普通二叉计划，空输入root=-1。
MergePlan buildMergePlan(const RunFile& file, const std::vector<int>& runs) {
    MergePlan plan;
    std::vector<int> level;
    for (size_t index = 0; index < runs.size(); ++index) {
        MergeNode leaf;
        leaf.runIndex = runs[index];
        leaf.length = file.run(runs[index]).count;
        level.push_back(static_cast<int>(plan.nodes.size()));
        plan.nodes.push_back(leaf);
    }
    while (level.size() > 1) {
        std::vector<int> next;
        for (size_t index = 0; index < level.size(); index += 2) {
            if (index + 1 == level.size()) { next.push_back(level[index]); continue; }
            MergeNode node;
            node.left = level[index];
            node.right = level[index + 1];
            node.length = plan.nodes[node.left].length + plan.nodes[node.right].length;
            plan.cost += node.length;
            next.push_back(static_cast<int>(plan.nodes.size()));
            plan.nodes.push_back(node);
        }
        level.swap(next);
    }
    if (!level.empty()) plan.root = level[0];
    return plan;
}
// 两路归并只保留两个输入块、一个输出块，以及两个当前值。
// 合并leftRun/rightRun两个已排序段，返回新段目录下标。
// 两路相等时先取左路；一方耗尽后顺序排空另一方。
int twoWayMerge(RunFile& file, int leftRun, int rightRun, size_t blockSize, IoStats& stats) {
    InputBuffer left(file, leftRun, blockSize, &stats);
    InputBuffer right(file, rightRun, blockSize, &stats);
    file.beginRun("M" + std::to_string(file.scan().size()));
    OutputBuffer output(file, blockSize, &stats);
    int32_t leftValue = 0, rightValue = 0;
    bool hasLeft = left.next(leftValue), hasRight = right.next(rightValue);
    while (hasLeft || hasRight) {
        if (hasLeft && (!hasRight || leftValue <= rightValue)) {
            output.writeValue(leftValue);
            hasLeft = left.next(leftValue);
        } else {
            output.writeValue(rightValue);
            hasRight = right.next(rightValue);
        }
    }
    output.flush();
    ++stats.merges;
    return file.endRun();
}
// 后序执行：叶子已有数据；内部节点等两个孩子完成后再归并。
// 执行nodeIndex子树并缓存结果段号；空树返回-1，已有结果直接复用。
int executeMergePlan(MergePlan& plan, int nodeIndex, RunFile& file,
                     size_t blockSize, IoStats& stats, std::ostream* trace) {
    if (nodeIndex == -1) return -1;
    MergeNode& node = plan.nodes.at(static_cast<size_t>(nodeIndex));
    if (node.runIndex >= 0) return node.runIndex;
    const int left = executeMergePlan(plan, node.left, file, blockSize, stats, trace);
    const int right = executeMergePlan(plan, node.right, file, blockSize, stats, trace);
    node.runIndex = twoWayMerge(file, left, right, blockSize, stats);
    if (trace) *trace << "merge " << file.run(left).name << " + " << file.run(right).name
                      << " -> " << file.run(node.runIndex).name << " length=" << node.length << '\n';
    return node.runIndex;
}
// 打印节点、孩子和理论加权长度cost，帮助核对计划与执行顺序。
void printMergePlan(const MergePlan& plan, std::ostream& output) {
    for (size_t index = 0; index < plan.nodes.size(); ++index) {
        const MergeNode& node = plan.nodes[index];
        output << "node " << index << " length=" << node.length;
        if (node.left < 0) output << " leaf run=" << node.runIndex;
        else output << " children=(" << node.left << ',' << node.right << ')';
        output << '\n';
    }
    output << "root=" << plan.root << " theoretical_cost=" << plan.cost << '\n';
}
// 哈夫曼思想：每次取两个最短段，最小化所有归并输出长度之和。
// 以段长为权构造最优二叉归并树；等长时按节点下标确定顺序。
// cost是元素搬移权重，不是含尾块取整和目录访问的实际I/O次数。
MergePlan buildOptimalPlan(const RunFile& file, const std::vector<int>& runs) {
    MergePlan plan;
    typedef std::pair<uint64_t, int> Item;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item> > queue;
    for (size_t index = 0; index < runs.size(); ++index) {
        MergeNode leaf;
        leaf.runIndex = runs[index];
        leaf.length = file.run(runs[index]).count;
        queue.push(Item(leaf.length, static_cast<int>(plan.nodes.size())));
        plan.nodes.push_back(leaf);
    }
    while (queue.size() > 1) {
        MergeNode node;
        node.left = queue.top().second; queue.pop();
        node.right = queue.top().second; queue.pop();
        node.length = plan.nodes[node.left].length + plan.nodes[node.right].length;
        plan.cost += node.length;
        queue.push(Item(node.length, static_cast<int>(plan.nodes.size())));
        plan.nodes.push_back(node);
    }
    if (!queue.empty()) plan.root = queue.top().second;
    return plan;
}
