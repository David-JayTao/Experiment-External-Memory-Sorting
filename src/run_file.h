#ifndef RUN_FILE_H
#define RUN_FILE_H
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// 只计数据块的实际 read/write 调用；文件头、目录和验证读取不计入排序指标。
struct IoStats {
    uint64_t readBlocks = 0;
    uint64_t writeBlocks = 0;
    uint64_t merges = 0;
    uint64_t rounds = 0;
    uint64_t stages = 0;
};
struct FileHeader {
    char magic[8];                 // 文件标识 EMSORT1
    uint32_t maxRuns;              // 固定目录容量
    uint32_t runCount;             // 已提交的目录项数量
    uint64_t directoryOffset;      // 字节偏移
    uint64_t dataOffset;
};
struct RunInfo {
    char name[32];                 // 最多 31 字节，末尾为零
    uint64_t offset;               // 第一个 int32_t 的文件字节偏移
    uint64_t count;                // 元素数，不是字节数
};
static_assert(sizeof(FileHeader) == 32, "FileHeader layout must be 32 bytes");
static_assert(sizeof(RunInfo) == 48, "RunInfo layout must be 48 bytes");

// 顺序追加归并段；同一对象的文件流只允许一个线程访问。
class RunFile {
public:
    static void create(const std::string& path, uint32_t maxRuns);
    explicit RunFile(const std::string& path, bool writable = true);
    const FileHeader& header() const { return header_; }
    const std::vector<RunInfo>& scan() const { return runs_; }
    int search(const std::string& name) const;
    const RunInfo& run(int index) const;
    void beginRun(const std::string& name);
    void writeBlock(const int32_t* values, size_t count, IoStats* stats = 0);
    int endRun();
    int put(const std::string& name, const std::vector<int32_t>& values);
    size_t readBlock(int index, uint64_t position, int32_t* values,
                     size_t capacity, IoStats* stats = 0);
    const std::string& path() const { return path_; }
private:
    std::string path_;
    std::fstream file_;            // seekg 读，seekp 写
    FileHeader header_;
    std::vector<RunInfo> runs_;    // 仅目录常驻内存，数据不常驻
    RunInfo pending_;             // 正在追加的段，endRun 后才可 search
    uint64_t endOffset_;          // 下一次追加位置
    bool writing_;
    bool writable_;
};

// 原始输入仅含连续 int32_t，没有 Header/Directory。
class RawInput {
public:
    explicit RawInput(const std::string& path);
    uint64_t count() const { return count_; }
    size_t readBlock(int32_t* values, size_t capacity, IoStats* stats = 0);
private:
    std::ifstream file_;
    uint64_t count_;
    uint64_t position_;
};
void writeRaw(const std::string& path, const std::vector<int32_t>& values);
#endif
