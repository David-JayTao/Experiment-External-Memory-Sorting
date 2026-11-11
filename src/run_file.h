#ifndef RUN_FILE_H
#define RUN_FILE_H
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// 只计数据块的实际 read/write 调用；文件头、目录和验证读取不计入排序指标。
struct IoStats {
    uint64_t readBlocks = 0; // 实际非空数据块读取调用次数
    uint64_t writeBlocks = 0; // 实际非空数据块写入调用次数
    uint64_t merges = 0; // 生成新段的归并操作次数
    uint64_t rounds = 0; // K路分组归并轮数；二叉计划不记此项
    uint64_t stages = 0; // 置换选择与K路归并的三线程同步阶段总数
};
// 固定32字节文件头，记录目录容量和数据起点；结构采用本机字节序。
struct FileHeader {
    char magic[8];                 // 文件标识 EMSORT1
    uint32_t maxRuns;              // 固定目录容量
    uint32_t runCount;             // 已提交的目录项数量
    uint64_t directoryOffset;      // 字节偏移
    uint64_t dataOffset; // 数据区首字节位置
};
// 固定48字节目录项；用名称、字节偏移和元素数描述一个连续归并段。
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
    // 创建文件并预留目录；已有同名文件会被截断。
    static void create(const std::string& path, uint32_t maxRuns);
    explicit RunFile(const std::string& path, bool writable = true);
    const FileHeader& header() const { return header_; }
    // Scan直接返回内存目录；只含endRun已经提交的段。
    const std::vector<RunInfo>& scan() const { return runs_; }
    int search(const std::string& name) const;
    const RunInfo& run(int index) const;
    // 开启一个待写段，随后可多次writeBlock，最后必须endRun提交。
    void beginRun(const std::string& name);
    void writeBlock(const int32_t* values, size_t count, IoStats* stats = 0);
    int endRun();
    // 一次性写入小段的便捷接口；正式排序应使用流式追加。
    int put(const std::string& name, const std::vector<int32_t>& values);
    // 按段内元素位置读取，最多capacity个；返回实际数量，0表示段末。
    size_t readBlock(int index, uint64_t position, int32_t* values,
                     size_t capacity, IoStats* stats = 0);
    const std::string& path() const { return path_; }
private:
    std::string path_; // 文件路径，供K路归并另开只读流
    std::fstream file_;            // seekg 读，seekp 写
    FileHeader header_; // 当前文件头的内存副本
    std::vector<RunInfo> runs_;    // 仅目录常驻内存，数据不常驻
    RunInfo pending_;             // 正在追加的段，endRun 后才可 search
    uint64_t endOffset_;          // 下一次追加位置
    bool writing_; // 是否存在尚未提交的待写段
    bool writable_; // 是否允许追加与提交
};

// 原始输入仅含连续 int32_t，没有 Header/Directory。
class RawInput {
public:
    explicit RawInput(const std::string& path);
    uint64_t count() const { return count_; }
    size_t readBlock(int32_t* values, size_t capacity, IoStats* stats = 0);
private:
    std::ifstream file_; // 原始输入的独立顺序读流
    uint64_t count_; // 文件包含的整数总数
    uint64_t position_; // 已经读取的整数数量
};
void writeRaw(const std::string& path, const std::vector<int32_t>& values);
#endif
