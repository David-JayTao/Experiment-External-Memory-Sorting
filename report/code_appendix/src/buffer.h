#ifndef BUFFER_H
#define BUFFER_H
#include "run_file.h"

// 算法逐个取值；内部只有一个固定容量块。
class InputBuffer {
public:
    InputBuffer(RunFile& file, int runIndex, size_t blockSize, IoStats* stats = 0);
    // 取得下一值；返回false表示段末，value此时不作为新数据。
    bool next(int32_t& value);
private:
    RunFile& file_; // 绑定的段文件，不拥有其生命周期
    int runIndex_; // 当前输入段的目录下标
    std::vector<int32_t> data_; // 容量为B的物理整数块
    uint64_t nextOffset_;          // 下一块在段内的元素下标
    size_t pos_;                  // 当前块下一个元素下标
    size_t count_;                // 当前块有效元素数
    IoStats* stats_; // 可选I/O计数器；验证读取可传空指针
};
// 逐元素接收排序结果并按块交给RunFile；与InputBuffer共同隔离算法和I/O。
class OutputBuffer {
public:
    OutputBuffer(RunFile& file, size_t blockSize, IoStats* stats = 0);
    void writeValue(int32_t value);
    void flush();                 // 调用者在 endRun 前显式刷新尾块
private:
    RunFile& file_; // 绑定的段文件，不拥有其生命周期
    std::vector<int32_t> data_; // 容量为B的物理整数块
    size_t count_; // 当前输出块已填入的有效整数数
    IoStats* stats_; // 可选I/O计数器；验证读取可传空指针
};
#endif
