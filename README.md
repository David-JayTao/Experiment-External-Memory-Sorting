# 外存排序课程实践

一个持续演进的 C++11 工程，实现东南大学《数据结构与算法专题实践》的三个外存排序项目。只用标准库，无第三方运行依赖。

- **Project 1**：按容量分块内排 → 普通二叉归并计划 → 双输入、单输出缓冲的二路归并。
- **Project 2**：败者树 + 冻结机制 + 三线程双缓冲置换选择 → 哈夫曼最优二路计划。
- **Project 3**：复用置换选择 → 败者树 K 路归并 → 共享 2k 输入块、两块输出、动态预取、多轮归并。

## 编译与运行

在有 C++11 编译器、CMake 3.16 或更高版本的终端中：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
```

Linux / WSL：

```sh
cd build
./external_sort week3
./external_sort project1
./external_sort project2
./external_sort project3 --k 3
./external_sort project3 --size 50003 --memory 256 --block 64 --k 4
./external_sort test
./external_sort benchmark
```

Windows 使用 Visual Studio 多配置生成器时，在仓库根目录运行 `build\Release\external_sort.exe project1`；MinGW 单配置生成器通常生成 `build\external_sort.exe`。本次已实测的编译环境为 Windows 上的 WSL2 + GCC，原生 MSVC 未实测。完整安装/VSCode 使用说明见讲义第八章。

## 入口与文档

先看 [从零看懂整个项目](docs/从零看懂整个项目.md)，再看 [验收演示与问答](docs/验收演示与问答.md)。[实验结果与总结](docs/实验结果与总结.md) 记录真实测试数据、指标口径及局限；[课程要求与实现对应](docs/课程要求与实现对应.md) 说明课程依据和设计取舍。

统一入口是 [main.cpp](main.cpp)，正式算法入口为 [sortFile](src/projects.cpp)，测试入口为 [runTests](tests/test_sort.cpp)。头文件列接口，源文件给实现，核心注释使用中文。

`--memory M` 和 `--block B` 的单位都是 **int32 元素个数**。M 是初始内排/候选数量，不是整个进程内存上限。目录、计划、败者树和输入输出缓冲另占空间。

无 `--size` 或 `--input` 时自动运行 15 个数的详细演示。`--size N` 用固定种子生成数据并精确对照 `std::sort`。读取自己的原始二进制文件：

```sh
./external_sort project3 --input data.bin --output result_runs.bin --memory 256 --block 64 --k 4
```

原始文件只含本机字节序的连续 `int32_t`，不能输入文本或已经包含目录的归并文件。输入和输出必须是两个独立文件；输出路径会被重新创建。自定义输入仅流式检查数量及非降序；完整多重集合一致性由测试模式和自动生成数据模式精确验证。

结果文件包含 Header + Directory + Data，保留初始段及中间段。屏幕输出 `final_run` 目录下标和名称，只有它是最终结果；不能把整个结果文件当作裸整数数组读取。空输入返回 `final_run=-1`。

## 阶段与测试

`week3-complete` 至 `week8-complete` 是本次开发的功能检查点。它们保留当时的可编译版本，不表示过去某周的提交日期。早期阶段仅支持当时已完成的模式。

完整测试含 220 次端到端精确比对、全部中间段有序检查、文件/尾块检查、败者树更新与冻结检查、最优树代价检查、动态块跨路回收检查，以及命令行非法参数检查。`benchmark` 输出少量 CSV 数据，可直接重跑；时间受平台、文件缓存和线程开销影响。

程序在当前工作目录创建演示/测试 `.bin` 文件；不同进程请使用不同工作目录。构建、临时数据及日志已被忽略。课程 PPT、录音、原讲稿仅作本地参考，不发布到公开仓库。
