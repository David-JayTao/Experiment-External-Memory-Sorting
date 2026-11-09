#ifndef BUFFER_H
#define BUFFER_H
#include "run_file.h"

// 算法逐个取值；内部只有一个固定容量块。
class InputBuffer {
public:
    InputBuffer(RunFile& file, int runIndex, size_t blockSize, IoStats* stats = 0);
    bool next(int32_t& value);
private:
    RunFile& file_;
    int runIndex_;
    std::vector<int32_t> data_;
    uint64_t nextOffset_;          // 下一块在段内的元素下标
    size_t pos_;                  // 当前块下一个元素下标
    size_t count_;                // 当前块有效元素数
    IoStats* stats_;
};
class OutputBuffer {
public:
    OutputBuffer(RunFile& file, size_t blockSize, IoStats* stats = 0);
    void writeValue(int32_t value);
    void flush();                 // 调用者在 endRun 前显式刷新尾块
private:
    RunFile& file_;
    std::vector<int32_t> data_;
    size_t count_;
    IoStats* stats_;
};
#endif
