#include "kway_merge.h"
#include <stdexcept>

BufferPool::BufferPool(size_t ways, size_t blockSize, const std::vector<RunInfo>& runs)
    : blocks_(2 * ways), queues_(runs.size()), next_(runs.size(), 0),
      lengths_(runs.size()), lastKeys_(runs.size(), 0) {
    if (ways < 2 || runs.size() > ways || !blockSize) throw std::runtime_error("invalid k-way buffers");
    for (size_t index = 0; index < blocks_.size(); ++index) {
        blocks_[index].values.resize(blockSize);
        free_.push(static_cast<int>(index));
    }
    for (size_t lane = 0; lane < runs.size(); ++lane) lengths_[lane] = runs[lane].count;
}
void BufferPool::initialization(RunFile& source, const std::vector<int>& runs, IoStats& stats) {
    for (size_t lane = 0; lane < runs.size(); ++lane) {
        if (lengths_[lane] == 0) continue;
        const int block = reserve();
        readPrefetch(source, runs[lane], static_cast<int>(lane), block, stats);
        finishPrefetch(static_cast<int>(lane), block);
    }
}
int BufferPool::choosePrefetch() const {
    int target = -1;
    for (size_t lane = 0; lane < queues_.size(); ++lane) {
        if (next_[lane] == lengths_[lane]) continue;
        // 空队列先补足；正常预取取已读末值最小的一路。
        if (queues_[lane].empty()) return static_cast<int>(lane);
        if (target < 0 || lastKeys_[lane] < lastKeys_[target]) target = static_cast<int>(lane);
    }
    return target;
}
int BufferPool::reserve() {
    if (free_.empty()) return -1;
    const int block = free_.front();
    free_.pop();
    return block;
}
void BufferPool::readPrefetch(RunFile& source, int runIndex, int lane, int block, IoStats& stats) {
    InputBlock& input = blocks_.at(static_cast<size_t>(block));
    input.pos = 0;
    input.count = source.readBlock(runIndex, next_.at(static_cast<size_t>(lane)), input.values.data(), input.values.size(), &stats);
    if (!input.count) throw std::runtime_error("unexpected empty prefetch");
}
void BufferPool::finishPrefetch(int lane, int block) {
    const InputBlock& input = blocks_[block];
    queues_[lane].push(block);
    next_[lane] += input.count;
    lastKeys_[lane] = input.values[input.count - 1];
}
bool BufferPool::hasValue(int lane) const { return !queues_.at(static_cast<size_t>(lane)).empty(); }
bool BufferPool::needsData(int lane) const { return !hasValue(lane) && next_[lane] < lengths_[lane]; }
int32_t BufferPool::front(int lane) const {
    const InputBlock& input = blocks_.at(static_cast<size_t>(queues_.at(static_cast<size_t>(lane)).front()));
    return input.values[input.pos];
}
void BufferPool::consume(int lane) {
    const int block = queues_[lane].front();
    InputBlock& input = blocks_[block];
    if (++input.pos == input.count) {
        queues_[lane].pop();
        released_.push_back(block);
    }
}
void BufferPool::releaseConsumed() {
    for (size_t index = 0; index < released_.size(); ++index) {
        blocks_[released_[index]].pos = blocks_[released_[index]].count = 0;
        free_.push(released_[index]);
    }
    released_.clear();
}
void BufferPool::print(std::ostream& output) const {
    output << "pool free=" << free_.size() << '/' << blocks_.size();
    for (size_t lane = 0; lane < queues_.size(); ++lane) {
        output << " lane" << lane << "=[";
        std::queue<int> queue = queues_[lane];
        while (!queue.empty()) {
            int block = queue.front(); queue.pop();
            output << block << ':' << blocks_[block].pos << '/' << blocks_[block].count << ' ';
        }
        output << "] loaded=" << next_[lane] << '/' << lengths_[lane] << " last=" << lastKeys_[lane];
    }
    output << '\n';
}
