#ifndef KWAY_MERGE_H
#define KWAY_MERGE_H
#include "run_file.h"
#include <queue>
#include <ostream>

// 共享池的一个物理块：整数数组及有效范围，不固定归属于某一路。
struct InputBlock {
    std::vector<int32_t> values; // 固定B容量的数据区
    size_t count = 0; // 本块有效整数数，尾块可小于B
    size_t pos = 0; // 下一个待消费位置
};
// 固定 2k 个物理块，逻辑上按路排队；块没有永久所属路。
class BufferPool {
public:
    BufferPool(size_t ways, size_t blockSize, const std::vector<RunInfo>& runs);
    // 每个非空段先读一块；其余物理块留在共享空闲池。
    void initialization(RunFile& source, const std::vector<int>& runs, IoStats& stats);
    // 返回待预取的路号：优先补空队列，否则选已读末值最小者；无任务返回 -1。
    int choosePrefetch() const;
    // 主线程保留一个空闲物理块，未完成读取前不加入计算队列。
    int reserve();
    // 输入线程只写被保留的块；同步后 finishPrefetch 才公开它。
    void readPrefetch(RunFile& source, int runIndex, int lane, int block, IoStats& stats);
    void finishPrefetch(int lane, int block);
    bool hasValue(int lane) const;
    bool needsData(int lane) const;
    int32_t front(int lane) const;
    // 计算线程消费一个值，耗尽的块先登记，join 后统一归还。
    void consume(int lane);
    void releaseConsumed();
    void print(std::ostream& output) const;
    size_t freeCount() const { return free_.size(); }
    size_t blockCount() const { return blocks_.size(); }
private:
    std::vector<InputBlock> blocks_; // 固定分配，线程运行时不 resize
    std::vector<std::queue<int> > queues_; // 每路的块编号队列
    std::queue<int> free_;          // 仅主线程在同步点改动
    std::vector<int> released_;     // 计算线程记下已耗尽块，同步后归还
    std::vector<uint64_t> next_;    // 每路已读入的元素数，主线程更新
    std::vector<uint64_t> lengths_; // 每条输入路的总元素数，用于判断真正读完
    std::vector<int32_t> lastKeys_; // 每路最后一个已读关键字，预取依据
};
// 合并一组至多ways段并返回结果目录下标；双输出配合三线程阶段同步。
int kWayMerge(RunFile& file, const std::vector<int>& runs, size_t ways,
              size_t blockSize, IoStats& stats, std::ostream* trace = 0);
// 多轮分组K路归并；返回最终段号，空输入返回-1。
int mergeKRuns(RunFile& file, std::vector<int> runs, size_t ways,
               size_t blockSize, IoStats& stats, std::ostream* trace = 0);
#endif
