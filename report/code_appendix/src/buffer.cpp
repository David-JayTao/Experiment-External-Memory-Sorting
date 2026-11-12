#include "buffer.h"
#include <stdexcept>
// 绑定一个已存在的段并分配B个整数；构造时校验容量和段号。
InputBuffer::InputBuffer(RunFile& file, int runIndex, size_t blockSize, IoStats* stats)
    : file_(file), runIndex_(runIndex), data_(blockSize), nextOffset_(0), pos_(0), count_(0), stats_(stats) {
    if (blockSize == 0) throw std::runtime_error("block size must be positive");
    file_.run(runIndex);
}
// 向value交付一个整数并返回true；段耗尽返回false。
// 仅当当前块全部消费后才补块，nextOffset_始终是段内元素下标。
bool InputBuffer::next(int32_t& value) {
    if (pos_ == count_) {
        count_ = file_.readBlock(runIndex_, nextOffset_, data_.data(), data_.size(), stats_);
        nextOffset_ += count_;
        pos_ = 0;
        if (count_ == 0) return false;
    }
    value = data_[pos_++];
    return true;
}
// 绑定当前待写段的文件，分配B个整数的输出块；beginRun由调用者负责。
OutputBuffer::OutputBuffer(RunFile& file, size_t blockSize, IoStats* stats)
    : file_(file), data_(blockSize), count_(0), stats_(stats) {
    if (blockSize == 0) throw std::runtime_error("block size must be positive");
}
// 缓存一个整数；满块立即写入，减少逐元素文件调用。
void OutputBuffer::writeValue(int32_t value) {
    data_[count_++] = value;
    if (count_ == data_.size()) flush();
}
// 写出count_个有效整数并清空计数；尾块需在endRun前显式调用。
void OutputBuffer::flush() {
    file_.writeBlock(data_.data(), count_, stats_);
    count_ = 0;
}
