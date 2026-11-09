#ifndef KWAY_MERGE_H
#define KWAY_MERGE_H
#include "run_file.h"
#include <queue>
#include <ostream>
struct InputBlock {
    std::vector<int32_t> values;
    size_t count = 0;
    size_t pos = 0;
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
    std::vector<uint64_t> lengths_;
    std::vector<int32_t> lastKeys_; // 每路最后一个已读关键字，预取依据
};
int kWayMerge(RunFile& file, const std::vector<int>& runs, size_t ways,
              size_t blockSize, IoStats& stats, std::ostream* trace = 0);
int mergeKRuns(RunFile& file, std::vector<int> runs, size_t ways,
               size_t blockSize, IoStats& stats, std::ostream* trace = 0);
#endif
