# 外存排序课程实践

C++11 + CMake 多文件工程，实现东南大学《数据结构与算法专题实践》三个外存排序项目。只用标准库，所有验收模式共用一个 main.cpp 和一个程序。

- **Project 1**：分块内排、普通二叉计划、双输入单输出二路归并。
- **Project 2**：败者树、冻结机制、三线程双缓冲置换选择、哈夫曼最优二路计划。
- **Project 3**：真正的 K 路归并、共享 2k 输入块、双输出、动态预取、多轮归并。

## Windows 环境

主要路线为 **Windows 原生 MSVC + CMake + VS Code**。本机已有 Windows 11、VS Code 1.145.2、Git 2.55.0、Windows CMake 4.4.4、C/C++ 1.35.6、CMake Tools 1.24.42。安装与实测记录见 [Windows 环境与验收指南](docs/Windows环境与验收指南.md)。

新电脑需从 [微软官网](https://visualstudio.microsoft.com/downloads/) 安装 Visual Studio Build Tools 的 C++ 工具、MSVC x64/x86 和 Windows SDK；另备 CMake 3.21+。不必安装完整 Visual Studio IDE。扩展按 `.vscode/extensions.json` 推荐安装。

脚本通过 vswhere 和 CMake 能力列表选择实际安装的 VS 生成器，不写死 VS 年份，也不要求 Developer PowerShell。本机复用用户目录 `miniforge3/Scripts/cmake.exe`。脚本还查找 PATH、标准 CMake 和 VS 自带 CMake；换电脑时，如 CMake Tools 报路径无效，请将工作区 `cmake.cmakePath` 改为当地路径，已在 PATH 时填 `cmake`。

## 最简单的终端验收

在仓库根目录的 PowerShell 中执行：

```powershell
.\验收.ps1 week3
.\验收.ps1 project1
.\验收.ps1 project2
.\验收.ps1 project3
.\验收.ps1 project3 -K 2
.\验收.ps1 project3 -K 4
.\验收.ps1 project3 -K 4 -Large
.\验收.ps1 test
.\验收.ps1 benchmark -Configuration Release
.\验收.ps1 ctest
.\验收.ps1 all
```

首次自动配置、编译，以后增量构建。默认 Debug，适合断点；计时选择 Release。`all` 顺序执行十项（包括 k=2/3/4、大规模测试、CTest），保留原始 trace，依据实际退出码打印 PASS/FAIL，失败返回非零。

默认 Project 演示只有15个数；50,003个数仅在 `-Large` 入口运行。程序分别为 `build/windows/Debug/external_sort.exe`、`build/windows/Release/external_sort.exe`。演示数据统一在 `build/windows/run/Debug` 或 `Release`，CTest 数据在构建目录。同一运行目录不要同时启动多个测试进程。

若终端阻止脚本，只对本次子进程使用下面的命令，不修改全局执行策略：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\验收.ps1 all
```

## VS Code 一键运行与调试

1. “文件 → 打开文件夹”选择仓库根目录，确认能看到 `CMakeLists.txt`，使用普通 Windows 窗口。
2. 首次先运行 `.\验收.ps1 build`。按 `Ctrl+Shift+P` → **CMake: Select Configure Preset** → `windows`；再执行 **CMake: Configure**，然后 **CMake: Select Build Preset** → `windows-debug`。预设模式不必另选 Kit。
3. 按 `Ctrl+Shift+D` 打开 **运行和调试 / Run and Debug**，在顶部下拉框选择 `验收：…`，共九项。
4. 点击**下拉框旁的绿色三角形**或按 F5。先自动构建 Debug，再在下方 **终端 / Terminal** 输出结果。不调试运行按 Ctrl+F5。
5. 打开 `src/kway_merge.cpp`，在 `const int lane = pool.choosePrefetch();` 行左边点红点，选 Project 3 k=3，按 F5。F10单步，F11进入函数，F5继续，Shift+F5停止。

九项包括 Week 3、Project 1、Project 2、Project 3 k=3/k=2/k=4、大规模 Project 3、Week 8测试和 Benchmark。均使用同一 Debug 程序和 `cppvsdbg`；VS Code 的 Benchmark 是 Debug 观察入口，正式计时用终端 Release 命令。

不要使用编辑器右上角的 **Run C/C++ File**：它通常只编译当前文件，不能替代这里的多文件工程配置。F5构建任务自带工具检测，不依赖扩展当前选择的 launch target。

## Week 3–8 验收对应

| 阶段 | PowerShell 命令 | 观察点 | 源码和重点函数 |
|---|---|---|---|
| Week 3 | `.\验收.ps1 week3` | Header=32、Directory=32、Data=416；A/B/C不等长；Search C=2 | run_file.cpp：create/search/beginRun/endRun；buffer.cpp；week3Demo |
| Week 4 / Project 1 | `.\验收.ps1 project1` | 4个初始段、cost=30、3次归并、精确PASS | merge_plan.cpp：runGeneration/buildMergePlan/executeMergePlan/twoWayMerge |
| Week 5 / Project 2 | `.\验收.ps1 project2` | 冻结/解冻、段长7/7/1、普通cost=29、最优cost=23 | loser_tree.cpp：replaceWinner；replacement.cpp：replacementSelection；buildOptimalPlan |
| Week 6 | `.\验收.ps1 project3` | pool、各路块队列、prefetch lane、跨路回收 | kway_merge.cpp：BufferPool::choosePrefetch/consume/releaseConsumed |
| Week 7 / Project 3 | `.\验收.ps1 project3 -K 2` 和 `-K 4 -Large` | 真正K路、多轮、双输出、排空 | kway_merge.cpp：kWayMerge/mergeKRuns |
| Week 8 | `.\验收.ps1 test` / `benchmark` / `ctest` | 220次精确比较、机制检查、12行benchmark、6项CTest | tests/test_sort.cpp：runTests/checkSort/runBenchmarks |

逐步讲解和问题准备见 [Windows指南](docs/Windows环境与验收指南.md) 与 [验收演示与问答](docs/验收演示与问答.md)。

## Linux / WSL 可选路线

原有 C++11 + CMake 3.16+ 构建仍有效，使用独立目录：

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j 4
ctest --test-dir build-linux --output-on-failure
cd build-linux
./external_sort week3
./external_sort project1
./external_sort project2
./external_sort project3 --k 3
./external_sort project3 --size 50003 --memory 256 --block 64 --k 4
./external_sort test
./external_sort benchmark
```

WSL 大量微小I/O建议把构建目录放在 `$HOME/.cache/external-sort-build`，避免 `/mnt/c` 挂载开销。Linux程序与 Windows exe 不可混用。

## 项目讲义与测试文档

- [Windows 环境与验收指南](docs/Windows环境与验收指南.md)：工具安装、运行、调试、排错与实际验证。
- [从零看懂整个项目](docs/从零看懂整个项目.md)：完整算法讲义、源码阅读顺序和问答。
- [验收演示与问答](docs/验收演示与问答.md)：向老师讲解的顺序与重点。
- [实验结果与总结](docs/实验结果与总结.md)：原始 WSL 实验数据、指标口径和局限。
- [课程要求与实现对应](docs/课程要求与实现对应.md)：课程依据和设计取舍。

统一入口为 main.cpp，正式算法入口 sortFile，测试入口 runTests。`--memory M` / `--block B` 单位都是 **int32元素个数**；M只是初始内排/候选数量，目录、计划、树和缓冲另占空间。

读取自己的原始数据（在 exe 目录执行）：

```powershell
.\external_sort.exe project3 --input data.bin --output result_runs.bin --memory 256 --block 64 --k 4
```

输入必须是本机字节序的连续 int32_t，不能是文本或带目录的归并文件。输入输出必须独立，输出会重新创建。自定义输入仅流式检查数量与非降序；测试和自动生成数据与 std::sort 精确比较。结果包含 Header + Directory + Data，保留初始及中间段，只有输出指定的 final_run 是最终段；空输入为-1。

week3-complete 至 week8-complete 保留原阶段检查点及历史。本次 Windows 配置为增量提交。构建、日志、临时数据及本机工具不提交；课件、录音、原讲稿仅供本地参考。
