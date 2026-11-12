#include "kway_merge.h"
#include "loser_tree.h"
#include <algorithm>
#include <exception>
#include <thread>
#include <stdexcept>

// 按配置k分配2k个B容量物理块；逻辑队列数为本组实际段数（可小于k）。
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
// 每条非空路先填一块，建立可比较的头值；空段不占块。
void BufferPool::initialization(RunFile& source, const std::vector<int>& runs, IoStats& stats) {
    for (size_t lane = 0; lane < runs.size(); ++lane) {
        if (lengths_[lane] == 0) continue;
        const int block = reserve();
        readPrefetch(source, runs[lane], static_cast<int>(lane), block, stats);
        finishPrefetch(static_cast<int>(lane), block);
    }
}

//动态预取策略
//下一个空闲块应该拿去预取哪一路？
// 1. 优先为已经没有缓冲数据、但磁盘上还有未读数据的路补块。
// 2. 否则，在还没读完的路中，选择 lastKeys_ 最小的一路。
// 3. 如果所有路都读取完，返回 -1。
// 其中 lastKeys_[lane] 是该路最后一个已经读入的元素。

// 因为每路有序，末尾已读值比较小的路，后续数据相对更有可能较早参与归并。

///////////////////////////////////////////////////////////////////
// 只考虑尚未读完的路；空队列优先，否则取lastKeys最小路，平局取先者。
// 返回路号而非块号；返回-1表示所有数据已读入，不代表所有数据已输出。
int BufferPool::choosePrefetch() const 
{
    int target = -1;
    for (size_t lane = 0; lane < queues_.size(); ++lane) 
    {
        if (next_[lane] == lengths_[lane]) continue;

        // 空队列先补足；正常预取取已读末值最小的一路。
        if (queues_[lane].empty()) return static_cast<int>(lane);
        if (target < 0 || lastKeys_[lane] < lastKeys_[target]) target = static_cast<int>(lane);
    }
    return target;
}
// 从共享空闲队列取出物理块号；无空闲块返回-1，预取本阶段不启动。
int BufferPool::reserve() {
    if (free_.empty()) return -1;
    const int block = free_.front();
    free_.pop();
    return block;
}
// 输入线程向预留块读取该路下一段数据；此时不改逻辑队列和next_。
void BufferPool::readPrefetch(RunFile& source, int runIndex, int lane, int block, IoStats& stats) {
    InputBlock& input = blocks_.at(static_cast<size_t>(block));
    input.pos = 0;
    input.count = source.readBlock(runIndex, next_.at(static_cast<size_t>(lane)), input.values.data(), input.values.size(), &stats);
    if (!input.count) throw std::runtime_error("unexpected empty prefetch");
}
// join后发布已读块：入该路队列，推进已读数量，记录块末关键字。
void BufferPool::finishPrefetch(int lane, int block) {
    const InputBlock& input = blocks_[block];
    queues_[lane].push(block);
    next_[lane] += input.count;
    lastKeys_[lane] = input.values[input.count - 1];
}
// 队列非空表示存在已公开且尚未消费的头值。
bool BufferPool::hasValue(int lane) const { return !queues_.at(static_cast<size_t>(lane)).empty(); }
// 队列为空且磁盘仍有未读值，表示头值未知，计算必须等待补块。
bool BufferPool::needsData(int lane) const { return !hasValue(lane) && next_[lane] < lengths_[lane]; }
// 取路队首块的当前整数；调用前必须确认hasValue。
int32_t BufferPool::front(int lane) const {
    const InputBlock& input = blocks_.at(static_cast<size_t>(queues_.at(static_cast<size_t>(lane)).front()));
    return input.values[input.pos];
}
// 计算线程消费一个值；块耗尽则出队并登记，暂不放回空闲池。
void BufferPool::consume(int lane) 
{
    const int block = queues_[lane].front();
    InputBlock& input = blocks_[block];
    if (++input.pos == input.count) 
    {
        queues_[lane].pop();
        released_.push_back(block);
    }
}



// 主线程在join后归还耗尽块，使它们下一阶段可服务任意一路。
void BufferPool::releaseConsumed() 
{
    for (size_t index = 0; index < released_.size(); ++index) 
    {
        blocks_[released_[index]].pos = blocks_[released_[index]].count = 0;
        free_.push(released_[index]);
    }
    released_.clear();
}
// 只在同步点打印队列、块位置及lastKeys；复制队列避免破坏原顺序。
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
// 合并runs中的一组有序段，返回新增结果段号；ways限定最多路数。
// 双输出块交替，输入池共享；独立读写文件流避免并发修改同一fstream。
int kWayMerge(RunFile& file, const std::vector<int>& runs, size_t ways,
              size_t blockSize, IoStats& stats, std::ostream* trace) 
{
    if (runs.empty() || ways < 2 || runs.size() > ways || !blockSize)
        throw std::runtime_error("invalid k-way merge group");
    std::vector<RunInfo> infos;
    uint64_t total = 0;
    for (size_t lane = 0; lane < runs.size(); ++lane) 
    {
        infos.push_back(file.run(runs[lane]));
        total += infos.back().count;
    }


    //////////////////1、初始化缓冲池

    RunFile source(file.path(), false);//独立只读文件流，输入线程使用。

    BufferPool pool(ways, blockSize, infos);
    pool.initialization(source, runs, stats);

    file.beginRun("K" + std::to_string(file.scan().size()));


    std::vector<int32_t> outputs[2];
    outputs[0].reserve(blockSize); outputs[1].reserve(blockSize);
    int activeOutput = 0;
    uint64_t emitted = 0;
    while (emitted < total || !outputs[1 - activeOutput].empty()) {

        ////////////////2、选择预取路和物理块
        const int lane = pool.choosePrefetch();
        const int prefetchBlock = lane < 0 ? -1 : pool.reserve();


        const int writeOutput = 1 - activeOutput;
        outputs[activeOutput].clear();
        if (trace) {
            pool.print(*trace);
            *trace << "prefetch lane=" << lane << " block=" << prefetchBlock
                   << " computeOutput=" << activeOutput << " writeOutput=" << writeOutput << '\n';
        }



        //////////////////3、收集每路头值，建立败者树

        // 每阶段从已公开队首重建k叶败者树；缺少未知头值时本阶段只补数据。
        std::vector<Candidate> heads(runs.size());
        bool missingHead = false;
        for (size_t index = 0; index < runs.size(); ++index) {
            if (pool.needsData(static_cast<int>(index))) missingHead = true;
            if (pool.hasValue(static_cast<int>(index))) {
                heads[index].valid = true;
                heads[index].value = pool.front(static_cast<int>(index));
            }
        }
        // 此实现每阶段建树O(k)，阶段内替换冠军O(log k)，复杂度需计入两者。
        LoserTree tree(heads);
        std::exception_ptr errors[3];
        std::thread inputThread, computeThread, outputThread;
        try {
            // 预取只写被reserve隔离的块，尚未入队，计算线程不能看见它。
            inputThread = std::thread([&]() {
                try {
                    if (prefetchBlock >= 0) pool.readPrefetch(source, runs[lane], lane, prefetchBlock, stats);
                } catch (...) { errors[0] = std::current_exception(); }
            });
            // 计算独占活动输出与消费位置；胜出路补上新头后沿败者树路径更新。
            computeThread = std::thread([&]() {
                try {
                    if (missingHead) return; // 缺少一路的未知头值时，不能从其他路继续输出。
                    while (outputs[activeOutput].size() < blockSize && tree.winner() >= 0) {
                        
                        /////////////////////4、三线程并行执行

                                // 这段就是完整的 K 路归并内核：
                                // 1. 找到最小值来自哪一路。
                                // 2. 将这个数加入输出块。
                                // 3. 从获胜路消费一个数。
                                // 4. 用该路的新头值替换败者树冠军。
                                // 5. 重复直到输出块满、没有可用值，或者需要等待补块。
                                                
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
            // 输出线程写上一阶段完成的块，不能读取本阶段正在生成的块。
            outputThread = std::thread([&]() {
                try { file.writeBlock(outputs[writeOutput].data(), outputs[writeOutput].size(), &stats); }
                catch (...) { errors[2] = std::current_exception(); }
            });
        } catch (...) {
            // 若创建后续线程失败，先收回已启动线程，避免析构时 terminate。
            if (inputThread.joinable()) inputThread.join();
            if (computeThread.joinable()) computeThread.join();
            if (outputThread.joinable()) outputThread.join();
            throw;
        }



        ////////////////////////////////5、阶段末尾同步
        // 三个任务结束才回收、公开预取块和切换输出；消除阶段间读写冲突。
        inputThread.join(); computeThread.join(); outputThread.join();
        for (int index = 0; index < 3; ++index) if (errors[index]) std::rethrow_exception(errors[index]);
        
        ++stats.stages;

        pool.releaseConsumed();

        if (prefetchBlock >= 0) pool.finishPrefetch(lane, prefetchBlock);
        /////////


        if (trace) *trace << "kway sync " << stats.stages << " emitted=" << emitted << '/' << total
                          << " produced=" << outputs[activeOutput].size() << " free=" << pool.freeCount() << '\n';
        outputs[writeOutput].clear();
        activeOutput = writeOutput;
    }
    ++stats.merges;
    return file.endRun();
}


///////////kWayMerge() 只负责一次合并最多 K 个段。可是如果初始有 10 个段，k=3，一次不可能全部处理。


// 一轮按组归并，每组是真正的 K 路选择；余下单段直接带入下一轮。
// 反复按至多k段分组，返回唯一最终段；空输入返回-1。
// 每轮段数缩为ceil(R/k)，余下单段直接携带，不执行额外数据复制。
int mergeKRuns(RunFile& file, std::vector<int> runs, size_t ways,
               size_t blockSize, IoStats& stats, std::ostream* trace)
{
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
