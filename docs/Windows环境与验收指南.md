# Windows 环境与验收指南

面向第一次使用 C++ 多文件工程的同学：打开 VS Code，选择验收项即可运行；也可以只输入一条 PowerShell 命令。本次维护保留原算法、统一入口和 Week 3–8 历史标签。

## 1. 工具与本机环境

VS Code 是编辑器；MSVC 把多个 cpp 编译、链接成 exe；Windows SDK 提供系统头文件与库；CMake 读取 CMakeLists.txt 并生成 MSBuild 工程；C/C++ 扩展提供调试器；CMake Tools 提供配置选择和代码提示。只安装扩展并不等于安装编译器。

本机检查日期为 **2026-11-10**：

| 工具 | 原有环境 |
|---|---|
| Windows | Windows 11，版本 10.0.26200 |
| VS Code | 用户安装版 1.145.2 |
| Git | 2.55.0.windows.2 |
| CMake | Windows 版 4.4.4，用户目录 miniforge3/Scripts；无 profile 终端未加入 PATH |
| C/C++ | ms-vscode.cpptools 1.35.6 |
| CMake Tools | ms-vscode.cmake-tools 1.24.42 |
| 原生编译器 | 初始未发现可用 MSVC，也未发现常见目录中的 MinGW/MSYS2 |
| PowerShell 策略 | CurrentUser=RemoteSigned；未修改任何持久执行策略 |

本次新增 **Visual Studio 2022 Build Tools 17.14.44**，安装返回码0，vswhere 确认完整、可启动、不要求重启。仅选择 C++ 构建工作负载的必要组件、MSVC x64/x86 和 Windows 11 SDK，继续复用已有 CMake 和扩展。安装器 Microsoft Corporation 数字签名为 Valid，管理员安装经用户确认。

新电脑从 [微软下载页](https://visualstudio.microsoft.com/downloads/) 获取 Build Tools，选择 C++ 工具和 Windows SDK，不必安装完整 IDE。组件见 [官方组件目录](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools)，参数见 [官方安装示例](https://learn.microsoft.com/en-us/visualstudio/install/command-line-parameter-examples)。本次 Presets 需要 CMake 3.21+；原有不用 Presets 的 Linux 构建仍只需3.16+。

## 2. 一条命令怎样完成验收

在 VS Code 打开仓库根目录，菜单“终端 → 新建终端”，使用 PowerShell：

```powershell
.\验收.ps1 project1
```

流程为：定位 CMake → 首次检测实际安装的 VS → 配置 windows preset → 增量编译 → 检查 exe → 切换运行目录 → 执行验收 → 打印 PASS/FAIL。后续仍会快速配置和增量构建，源码没改不会全部重编译。配置、编译失败后停止，不会启动旧程序。

| 目的 | 命令 |
|---|---|
| 文件和缓冲区 | `.\验收.ps1 week3` |
| 二路外排序 | `.\验收.ps1 project1` |
| 置换选择和最优树 | `.\验收.ps1 project2` |
| k=3小演示 | `.\验收.ps1 project3` |
| k=2 / k=4小演示 | `.\验收.ps1 project3 -K 2` / `-K 4` |
| 50,003个数 | `.\验收.ps1 project3 -K 4 -Large` |
| 全部正确性测试 | `.\验收.ps1 test` |
| 正式性能计时 | `.\验收.ps1 benchmark -Configuration Release` |
| 六项CTest | `.\验收.ps1 ctest` |
| 顺序运行全部十项 | `.\验收.ps1 all` |
| 仅构建调试程序 | `.\验收.ps1 build` |

默认 Debug，所有模式都可加 `-Configuration Release`。all 包含 Week 3、Project 1、Project 2、Project 3 k=3/2/4、大规模 Project 3、test、benchmark、CTest。单项失败仍继续其他验收，最终实际汇总；有 FAIL 则脚本退出码为1。

执行策略阻止脚本时，只对本次子进程使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\验收.ps1 all
```

脚本和任务不调用 Set-ExecutionPolicy，不修改全局 PATH。脚本保存为 UTF-8 BOM，保证 Windows PowerShell 5.1 读取中文。

## 3. 程序和数据在哪里

| 内容 | 相对仓库的位置 |
|---|---|
| CMakeCache、MSBuild工程 | build/windows |
| Debug程序与符号 | build/windows/Debug/external_sort.exe 及 pdb |
| Release程序 | build/windows/Release/external_sort.exe |
| Debug演示数据 | build/windows/run/Debug |
| Release演示数据 | build/windows/run/Release |
| CTest数据和日志 | build/windows、build/windows/Testing |

脚本以自己的位置为根，因此从其他目录调用也能找到项目。算法使用相对文件名，运行目录很重要。同一目录不要并发启动 test、benchmark 或同名 Project，它们写相同文件；先结束前一个再启动下一个。

所有 build、日志、临时数据和安装器被 Git 忽略。原有 WSL 构建与 Windows 构建互不覆盖。

## 4. VS Code 实际点击步骤

### 第一次打开

1. “文件 → 打开文件夹”选择仓库根目录，能看到 CMakeLists.txt；使用普通 Windows 窗口。
2. 确认 Microsoft C/C++ 和 CMake Tools 已启用。
3. 首次执行 `.\验收.ps1 build`，让脚本选择实际存在的 VS。
4. `Ctrl+Shift+P` → **CMake: Select Configure Preset** → windows。
5. 执行 **CMake: Configure**，再执行 **CMake: Select Build Preset** → windows-debug。预设模式不必另选 Kit。

工作区 cmake.cmakePath 当前为 `${userHome}/miniforge3/Scripts/cmake.exe`，对应本机已有安装。换电脑时改为该电脑路径，或已在 PATH 时填 cmake。F5 自动构建使用脚本自己的查找逻辑，不依赖扩展当前 launch target。

本机实测发现 C/C++ 1.35.6 的 Ctrl+F5 会复用已有终端，未重新应用 cwd 和环境变量。九个配置因此同时设置 cwd，并在原有模式参数前加 `--workdir <运行目录>`；Windows 入口只切换本进程目录，不改变算法与普通命令行行为。该前缀的中文目录、缺参和无效目录均已检查。下表只列主要模式参数。

### 九个验收选项

按 `Ctrl+Shift+D`，在“运行和调试”的顶部下拉框中选择项目，然后点**下拉框旁绿色三角形**。

| 配置名 | 实际参数 |
|---|---|
| 验收：Week 3 文件与缓冲区 | week3 |
| 验收：Project 1 二路外排序 | project1 |
| 验收：Project 2 置换选择 | project2 |
| 验收：Project 3 K 路归并（k=3） | project3 --k 3 |
| 验收：Project 3（k=2） | project3 --k 2 |
| 验收：Project 3（k=4） | project3 --k 4 |
| 验收：Project 3 大规模测试 | project3 --size 50003 --memory 256 --block 64 --k 4 |
| 验收：Week 8 全部测试 | test |
| 验收：Benchmark | benchmark |

F5 调试，Ctrl+F5 不调试运行。每次先自动构建 Debug，随后启动同一个 exe，结果显示在下方 Terminal。若仍看到构建日志，在终端选择器切到程序终端。此处 Benchmark 使用 Debug；正式计时用终端 Release。

不要使用编辑器右上角 **Run C/C++ File**，它通常只编译当前 cpp，会漏掉其他源文件。任意三角形并不都等同于本项目的九个具名配置。

### 在核心函数停下来

1. 打开 src/kway_merge.cpp，找到 kWayMerge 中 `const int lane = pool.choosePrefetch();`，点行号左侧空白，出现红点。
2. 选择 Project 3 k=3，按 F5。停住后当前行高亮，左侧显示变量和调用堆栈。
3. F10 执行该行后看 lane，再 F10 后看 prefetchBlock；lane=-1 表示没有下一块需要预取。展开 pool，观察队列、空闲块，再看 emitted/total 和双输出切换。
4. F11 进入 choosePrefetch，Shift+F11 返回，F5 继续，Shift+F5 停止。

Project 2 在 replacementSelection、LoserTree::replaceWinner 观察 frozen；Project 1 在 executeMergePlan 观察递归调用栈。Debug保留符号；Release优化后变量不可见或行号跳跃不一定是错误。

## 5. 给老师演示什么

顺序为 Week 3 → Project 1 → Project 2 → Project 3 → test → benchmark。三个 Project 的默认小输入相同：

```text
input: 9 1 8 2 7 3 6 4 5 0 -1 3 12 2 10
output: -1 0 1 2 2 3 3 4 5 6 7 8 9 10 12
PASS std::sort exact comparison
```

| 阶段 | 预期输出与解释 | 老师可能检查的函数 |
|---|---|---|
| Week 3 | Header=32、Directory=32、Data=416；A=-5,2,8；B=0,1；C=3,4,5,6,7；Search C=2、missing=-1。目录记录偏移与长度；尾块显式flush。 | RunFile::create/search/beginRun/endRun，InputBuffer::next，OutputBuffer::flush |
| Week 4 | 4个初始段，cost=30，3次归并。建树只计划，后序执行才读写；两个输入块和一个输出块。 | runGeneration、buildMergePlan、executeMergePlan、twoWayMerge |
| Week 5 | losers存败者，冻结候选归下一段；段长7/7/1，普通cost=29、最优cost=23。三线程按阶段join交接。 | LoserTree::replaceWinner/thaw、replacementSelection、buildOptimalPlan |
| Week 6 | pool和每路队列显示共享输入池；prefetch lane为预取路，block为物理块；回收后可服务另一条路。 | BufferPool::choosePrefetch/reserve/consume/releaseConsumed |
| Week 7 | k=2/3/4共用真正K路选择；多段进行多轮，末组不足k也正常；双输出切换并最终排空。 | kWayMerge、mergeKRuns |
| Week 8 | 220次精确对比及机制测试；4个N×3个项目共12行benchmark。I/O减少不保证更快，建线程有成本。 | runTests、checkSort、runBenchmarks |

小演示保留全部trace；大规模不默认刷逐阶段信息。如需中等规模的多轮详细输出，在 `build/windows/run/Debug` 执行：

```powershell
& ..\..\Debug\external_sort.exe project3 --size 257 --memory 8 --block 16 --k 3 --trace
```

## 6. 常见问题

- 找不到CMake：需Windows版。脚本检查PATH、Program Files/CMake、用户miniforge3/miniconda3、CONDA_PREFIX和VS自带位置。CMake Tools路径设置与脚本查找独立。
- 找不到MSVC/SDK：在Visual Studio Installer检查C++工具和Windows SDK；C/C++扩展不能替代编译器。
- 生成器冲突：不要将WSL/MinGW缓存放入build/windows；将旧构建目录改名保留，再运行脚本，不要手改CMakeCache。
- F5找不到exe：查看自动构建终端第一条错误。修复后重试，不能跳过失败继续运行旧程序。
- 断点空心：确认本仓库具名配置、Debug、已保存的源码，以及该行确实执行；可先在main第一条语句打断点。
- 中文乱码：ps1用UTF-8 BOM，文档与JSON用UTF-8，终端使用支持中文的字体。MSVC已有/utf-8；算法trace本身是英文，便于跨平台比较。
- 测试较慢：大量小文件I/O、建线程及调试线程事件会增加时间；不要并发运行，性能实验用Release。
- Windows与WSL时间不同：缓存和线程调度不同。检查排序值、段数和逻辑块读写数，不要求毫秒相同；旧WSL结果表保留原始数据。

## 7. 实际验证记录

2026年11月10日本机实测：

- 微软签名有效的 Build Tools 安装程序退出码0；MSVC 19.44.35229、工具集14.44.35207、SDK 10.0.26100.0，可启动且无需重启。
- Windows 原生 Debug、Release 均完成十项验收：220次完整排序精确比较、k=2/3/4、50,003元素、12行Benchmark及6/6 CTest全部通过。
- 独立中文及空格源码路径从零配置、编译并运行Project 3通过；非法K返回非零。Windows工作目录前缀在中文目录、无效目录、缺少参数下分别得到预期结果。
- VS Code九个具名配置已实际启动并观察结果；绿色三角、F5、Ctrl+F5均完成实测。CMake Tools已选择windows与windows-debug预设并配置成功。
- 在kWayMerge的choosePrefetch语句设置断点，F5成功暂停，F10后局部变量lane=0，调用堆栈可见；随后清除临时断点并继续至精确比较PASS。
- Ctrl+F5复用终端实测：命令带--workdir，运行目录中的project3_input.bin时间更新，仓库根同名旧文件时间未变。PowerShell提示符仍可显示仓库根，它不是子进程的最终工作目录。
- 真实截图保存在本地忽略目录report/figures；含调试暂停的耗时不能作为性能数据。正式Windows Release单次计时另存benchmark-windows.csv，旧WSL数据保留不覆盖。

安装与验收日志保存在本地.local/windows目录，不提交安装包、构建产物及个人报告。
