#include "buffer.h"
#include <stdexcept>
InputBuffer::InputBuffer(RunFile& file, int runIndex, size_t blockSize, IoStats* stats)
    : file_(file), runIndex_(runIndex), data_(blockSize), nextOffset_(0), pos_(0), count_(0), stats_(stats) {
    if (blockSize == 0) throw std::runtime_error("block size must be positive");
    file_.run(runIndex);
}
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
OutputBuffer::OutputBuffer(RunFile& file, size_t blockSize, IoStats* stats)
    : file_(file), data_(blockSize), count_(0), stats_(stats) {
    if (blockSize == 0) throw std::runtime_error("block size must be positive");
}
void OutputBuffer::writeValue(int32_t value) {
    data_[count_++] = value;
    if (count_ == data_.size()) flush();
}
void OutputBuffer::flush() {
    file_.writeBlock(data_.data(), count_, stats_);
    count_ = 0;
}
