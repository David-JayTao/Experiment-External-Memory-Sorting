# 外存排序课程实验报告

正式 PDF：`外存排序课程实践实验报告_刘酝潇_09024211.pdf`。
主入口：`main.tex`；正文在 `chapters/`；真实截图与矢量性能图在 `figures/`。
本目录包含正式 PDF、可重新编译的 LaTeX 源文件、图表和源码快照。个人信息已按作者意愿随报告公开。

最终交付版本共65页：封面、摘要及目录5页，第一至第五章正文28页，第六章完整源码32页。包含5张真实Windows运行截图、3张矢量性能图（6个面板）和与实际实现对应的结构/流程示意图。源码对应提交 `478a7ec6bcc95793a0e5540225609df04986bae2`。
逐页检查及数据核对记录见 `evidence/最终交付检查.md`；正式PDF只需单独提交，源文件留作学习和重新编译。

## 重新编译

本机复用 TeX Live 2026 的 XeLaTeX，不需要安装新的发行版。正文使用 ctex、Fandol 中文字体，代码使用 Consolas 和 Fandol 仿宋；宏包均为常见 TeX Live 组件。

在仓库根 PowerShell 执行（需要 TeX Live 及 Python 3）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\report\scripts\build_report.ps1 -Render
```

脚本连续编译三遍以生成目录与交叉引用，校验快照和日志，选择性渲染所有页面，成功后复制最终命名 PDF。中间产物在 `build/`，逐页图片及联系表在 `build/review/`，检查摘要在 `build/verification.json`。
仅编译 LaTeX 也可在 report 目录运行三遍：

```powershell
& C:\texlive\2026\bin\windows\xelatex.exe -interaction=nonstopmode -halt-on-error -output-directory=build main.tex
```

Python脚本依赖 matplotlib、numpy、Pillow 和 PyMuPDF。构建脚本优先使用当前用户目录下的 Python，也可以使用 PATH 中的 Python；迁移电脑时安装这些依赖即可。TeX编译已有图和快照本身不需要Python。

仓库根目录的 `.gitignore` 只排除 `report/build/` 编译和逐页渲染产物；PDF、LaTeX、截图、CSV、源码快照、生成脚本和验收记录均纳入版本管理。

## 源码附录

`scripts/snapshot_code.py` 从仓库读取 main.cpp、src和tests下全部正式.h/.cpp，并核对与当前提交一致；自动复制18个文件（17个C++文件及CMakeLists.txt）、生成 listings.tex、version.tex 和 SHA-256 manifest.json。
在源码提交且验收完成后才运行：

```powershell
python .\report\scripts\snapshot_code.py
```

普通重新编译不刷新快照。这样可以保持报告与已验收版本一致；即使从仓库移走整个report目录，也能凭快照重新编译。若希望更新代码版本，需要先重新验收，再显式生成快照并检查正文中的实现分析是否仍正确。

## 图表与证据

- `data/benchmark-windows.csv`：Windows Release已保存的真实单次12行测量。
- `data/benchmark-wsl.csv`：此前WSL Release独立测量；不可混为同一平台。
- `scripts/make_charts.py`：读取上述CSV，生成3张矢量PDF（共6个面板）及表格TeX，并核对两平台非时间列相同。
- `figures/windows-*.png`：原始实际窗口截图；报告只使用LaTeX裁去无关区域，不重画终端内容。截图文件可能以JPEG编码保存在.png后缀中，LaTeX按实际图像内容识别。
- `evidence/`：Debug/Release验收日志及最终逐页检查记录。

截图用于展示机制和真实运行，含Debug或断点停留的时间不能作为性能数据。正式性能图使用数据快照，非平均值、非冷缓存受控实验。后续重跑时间变化正常。

## 质量检查

`verify_report.py`核对17个C++文件均被全文引入、各快照SHA-256一致、PDF没有空白页和明显文字出界、LaTeX没有缺字/未解析引用/溢出。`render_review.py`渲染所有页面供视觉核查；自动检查不能替代逐页检查图形、中文和代码断行。

报告采用老师要求的六章，完整源码始终在最后。只提交正式PDF时不需要把整个C++工程、编译产物或安装包发给老师。
