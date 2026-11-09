#include "kway_merge.h"
#include "loser_tree.h"
#include <algorithm>
#include <exception>
#include <thread>
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

// 合并这一组最多 k 个段，输出线程用 file，输入线程用独立 source 文件流。
int kWayMerge(RunFile& file, const std::vector<int>& runs, size_t ways,
              size_t blockSize, IoStats& stats, std::ostream* trace) {
    if (runs.empty() || ways < 2 || runs.size() > ways || !blockSize)
        throw std::runtime_error("invalid k-way merge group");
    std::vector<RunInfo> infos;
    uint64_t total = 0;
    for (size_t lane = 0; lane < runs.size(); ++lane) {
        infos.push_back(file.run(runs[lane]));
        total += infos.back().count;
    }
    RunFile source(file.path(), false);
    BufferPool pool(ways, blockSize, infos);
    pool.initialization(source, runs, stats);
    file.beginRun("K" + std::to_string(file.scan().size()));
    std::vector<int32_t> outputs[2];
    outputs[0].reserve(blockSize); outputs[1].reserve(blockSize);
    int activeOutput = 0;
    uint64_t emitted = 0;
    while (emitted < total || !outputs[1 - activeOutput].empty()) {
        const int lane = pool.choosePrefetch();
        const int prefetchBlock = lane < 0 ? -1 : pool.reserve();
        const int writeOutput = 1 - activeOutput;
        outputs[activeOutput].clear();
        if (trace) {
            pool.print(*trace);
            *trace << "prefetch lane=" << lane << " block=" << prefetchBlock
                   << " computeOutput=" << activeOutput << " writeOutput=" << writeOutput << '\n';
        }
        std::vector<Candidate> heads(runs.size());
        bool missingHead = false;
        for (size_t index = 0; index < runs.size(); ++index) {
            if (pool.needsData(static_cast<int>(index))) missingHead = true;
            if (pool.hasValue(static_cast<int>(index))) {
                heads[index].valid = true;
                heads[index].value = pool.front(static_cast<int>(index));
            }
        }
        LoserTree tree(heads);
        std::exception_ptr errors[3];
        std::thread inputThread([&]() {
            try {
                if (prefetchBlock >= 0) pool.readPrefetch(source, runs[lane], lane, prefetchBlock, stats);
            } catch (...) { errors[0] = std::current_exception(); }
        });
        std::thread computeThread([&]() {
            try {
                if (missingHead) return; // 缺少一路的未知头值时，不能从其他路继续输出。
                while (outputs[activeOutput].size() < blockSize && tree.winner() >= 0) {
                    const int winner = tree.winner();
                    outputs[activeOutput].push_back(tree.top().value);
                    ++emitted;
                    pool.consume(winner);
                    Candidate next;
                    if (pool.hasValue(winner)) {
                        next.valid = true;
                        next.value = pool.front(winner);
                    }
                    tree.replaceWinner(next);
                    if (pool.needsData(winner)) break;
                }
            } catch (...) { errors[1] = std::current_exception(); }
        });
        std::thread outputThread([&]() {
            try { file.writeBlock(outputs[writeOutput].data(), outputs[writeOutput].size(), &stats); }
            catch (...) { errors[2] = std::current_exception(); }
        });
        inputThread.join(); computeThread.join(); outputThread.join();
        for (int index = 0; index < 3; ++index) if (errors[index]) std::rethrow_exception(errors[index]);
        ++stats.stages;
        pool.releaseConsumed();
        if (prefetchBlock >= 0) pool.finishPrefetch(lane, prefetchBlock);
        if (trace) *trace << "kway sync " << stats.stages << " emitted=" << emitted << '/' << total
                          << " produced=" << outputs[activeOutput].size() << " free=" << pool.freeCount() << '\n';
        outputs[writeOutput].clear();
        activeOutput = writeOutput;
    }
    ++stats.merges;
    return file.endRun();
}

// 一轮按组归并，每组是真正的 K 路选择；余下单段直接带入下一轮。
int mergeKRuns(RunFile& file, std::vector<int> runs, size_t ways,
               size_t blockSize, IoStats& stats, std::ostream* trace) {
    if (ways < 2 || !blockSize) throw std::runtime_error("k must be at least two");
    while (runs.size() > 1) {
        ++stats.rounds;
        if (trace) *trace << "merge round " << stats.rounds << " runs=" << runs.size() << '\n';
        std::vector<int> next;
        for (size_t begin = 0; begin < runs.size(); begin += ways) {
            const size_t end = std::min(runs.size(), begin + ways);
            if (end - begin == 1) next.push_back(runs[begin]);
            else {
                std::vector<int> group(runs.begin() + begin, runs.begin() + end);
                next.push_back(kWayMerge(file, group, ways, blockSize, stats, trace));
            }
        }
        runs.swap(next);
    }
    return runs.empty() ? -1 : runs[0];
}
