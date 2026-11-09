#include "run_file.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

// 直接保存固定结构，使用本机字节序；本项目面向常见小端 64 位平台。
void RunFile::create(const std::string& path, uint32_t maxRuns) {
    if (maxRuns == 0) throw std::runtime_error("directory capacity must be positive");
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("cannot create " + path);
    FileHeader header = {};
    std::memcpy(header.magic, "EMSORT1", 8);
    header.maxRuns = maxRuns;
    header.directoryOffset = sizeof(FileHeader);
    header.dataOffset = sizeof(FileHeader) + uint64_t(maxRuns) * sizeof(RunInfo);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    RunInfo empty = {};
    for (uint32_t index = 0; index < maxRuns; ++index)
        file.write(reinterpret_cast<const char*>(&empty), sizeof(empty));
    file.flush();
    if (!file) throw std::runtime_error("cannot initialize " + path);
}

RunFile::RunFile(const std::string& path, bool writable)
    : path_(path), endOffset_(0), writing_(false), writable_(writable) {
    std::ios::openmode mode = std::ios::binary | std::ios::in;
    if (writable) mode |= std::ios::out;
    file_.open(path.c_str(), mode);
    if (!file_) throw std::runtime_error("cannot open " + path);
    file_.read(reinterpret_cast<char*>(&header_), sizeof(header_));
    if (!file_ || std::memcmp(header_.magic, "EMSORT1", 8) != 0 ||
        header_.maxRuns == 0 || header_.runCount > header_.maxRuns ||
        header_.directoryOffset != sizeof(FileHeader) ||
        header_.dataOffset != sizeof(FileHeader) + uint64_t(header_.maxRuns) * sizeof(RunInfo))
        throw std::runtime_error("invalid run file header");
    file_.seekg(0, std::ios::end);
    const std::streamoff fileSize = file_.tellg();
    if (fileSize < 0 || uint64_t(fileSize) < header_.dataOffset)
        throw std::runtime_error("truncated directory");
    runs_.resize(header_.runCount);
    file_.seekg(static_cast<std::streamoff>(header_.directoryOffset));
    endOffset_ = header_.dataOffset;
    for (size_t index = 0; index < runs_.size(); ++index) {
        RunInfo& info = runs_[index];
        file_.read(reinterpret_cast<char*>(&info), sizeof(info));
        if (!file_ || !std::memchr(info.name, 0, sizeof(info.name)) ||
            info.offset != endOffset_ || info.offset > uint64_t(fileSize) ||
            info.count > (uint64_t(fileSize) - info.offset) / sizeof(int32_t))
            throw std::runtime_error("invalid run directory entry");
        endOffset_ = info.offset + info.count * sizeof(int32_t);
    }
}
int RunFile::search(const std::string& name) const {
    for (size_t index = 0; index < runs_.size(); ++index)
        if (name == runs_[index].name) return static_cast<int>(index);
    return -1;
}
const RunInfo& RunFile::run(int index) const {
    if (index < 0 || size_t(index) >= runs_.size()) throw std::runtime_error("run not found");
    return runs_[index];
}
void RunFile::beginRun(const std::string& name) {
    if (!writable_ || writing_) throw std::runtime_error("cannot begin run in this state");
    if (runs_.size() >= header_.maxRuns) throw std::runtime_error("directory is full");
    if (name.empty() || name.size() >= sizeof(pending_.name) || name.find('\0') != std::string::npos || search(name) >= 0)
        throw std::runtime_error("invalid or duplicate run name");
    pending_ = RunInfo();
    std::memcpy(pending_.name, name.c_str(), name.size());
    pending_.offset = endOffset_;
    writing_ = true;
}
void RunFile::writeBlock(const int32_t* values, size_t count, IoStats* stats) {
    if (!writing_) throw std::runtime_error("beginRun is required");
    if (count == 0) return;
    file_.clear();
    file_.seekp(static_cast<std::streamoff>(endOffset_));
    file_.write(reinterpret_cast<const char*>(values), static_cast<std::streamsize>(count * sizeof(int32_t)));
    if (!file_) throw std::runtime_error("run data write failed");
    pending_.count += count;
    endOffset_ += count * sizeof(int32_t);
    if (stats) ++stats->writeBlocks;
}
// 先写数据，再写目录，最后公布 runCount；不提供掉电事务保证。
int RunFile::endRun() {
    if (!writing_) throw std::runtime_error("no pending run");
    file_.seekp(static_cast<std::streamoff>(header_.directoryOffset + runs_.size() * sizeof(RunInfo)));
    file_.write(reinterpret_cast<const char*>(&pending_), sizeof(pending_));
    ++header_.runCount;
    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
    file_.flush();
    if (!file_) throw std::runtime_error("run metadata write failed");
    runs_.push_back(pending_);
    writing_ = false;
    return static_cast<int>(runs_.size() - 1);
}
int RunFile::put(const std::string& name, const std::vector<int32_t>& values) {
    beginRun(name);
    writeBlock(values.data(), values.size());
    return endRun();
}
size_t RunFile::readBlock(int index, uint64_t position, int32_t* values, size_t capacity, IoStats* stats) {
    const RunInfo& info = run(index);
    if (capacity == 0 || position > info.count) throw std::runtime_error("invalid read range");
    const size_t count = static_cast<size_t>(std::min<uint64_t>(capacity, info.count - position));
    if (count == 0) return 0;
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(info.offset + position * sizeof(int32_t)));
    file_.read(reinterpret_cast<char*>(values), static_cast<std::streamsize>(count * sizeof(int32_t)));
    if (!file_) throw std::runtime_error("run data read failed");
    if (stats) ++stats->readBlocks;
    return count;
}
RawInput::RawInput(const std::string& path) : count_(0), position_(0) {
    file_.open(path.c_str(), std::ios::binary);
    if (!file_) throw std::runtime_error("cannot open raw input " + path);
    file_.seekg(0, std::ios::end);
    const std::streamoff bytes = file_.tellg();
    if (bytes < 0 || bytes % sizeof(int32_t) != 0) throw std::runtime_error("raw input is not whole int32 values");
    count_ = static_cast<uint64_t>(bytes) / sizeof(int32_t);
    file_.seekg(0);
}
size_t RawInput::readBlock(int32_t* values, size_t capacity, IoStats* stats) {
    if (capacity == 0) throw std::runtime_error("block size must be positive");
    const size_t count = static_cast<size_t>(std::min<uint64_t>(capacity, count_ - position_));
    if (count == 0) return 0;
    file_.read(reinterpret_cast<char*>(values), static_cast<std::streamsize>(count * sizeof(int32_t)));
    if (!file_) throw std::runtime_error("raw read failed");
    position_ += count;
    if (stats) ++stats->readBlocks;
    return count;
}
void writeRaw(const std::string& path, const std::vector<int32_t>& values) {
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("cannot create raw file");
    if (!values.empty()) file.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(int32_t)));
    file.flush();
    if (!file) throw std::runtime_error("raw write failed");
}
