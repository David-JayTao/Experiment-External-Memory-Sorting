"""只读报告内CSV快照生成矢量图；不重新运行benchmark或改写测量值。"""
from pathlib import Path
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
font = Path('C:/Windows/Fonts/msyh.ttc')
if font.exists():
    font_manager.fontManager.addfont(str(font))
    plt.rcParams['font.family'] = font_manager.FontProperties(fname=str(font)).get_name()
plt.rcParams.update({'font.size': 9, 'axes.unicode_minus': False, 'pdf.fonttype': 42,
    'axes.spines.top': False, 'axes.spines.right': False, 'axes.axisbelow': True})
with (ROOT / 'data/benchmark-windows.csv').open(encoding='utf-8-sig', newline='') as f:
    rows = list(csv.DictReader(f))
with (ROOT / 'data/benchmark-wsl.csv').open(encoding='utf-8-sig', newline='') as f:
    wsl = list(csv.DictReader(f))
assert len(rows) == len(wsl) == 12
for a, b in zip(rows, wsl):
    assert a['exact'] == b['exact'] == 'PASS'
    assert {k:v for k,v in a.items() if k!='ms'} == {k:v for k,v in b.items() if k!='ms'}
sizes = [31, 257, 4099, 50003]
colors = ['#244C66', '#438D91', '#CF8841']
def vals(project, key):
    return [float(r[key]) for r in rows if int(r['project']) == project]
def save(fig, name):
    fig.savefig(ROOT / 'figures' / name, bbox_inches='tight')
    plt.close(fig)

fig, axes = plt.subplots(1, 2, figsize=(7.1, 2.65), layout='constrained')
x = np.arange(4)
for ax, key, ylabel in zip(axes, ['runs','merges'], ['初始归并段数 / 个','实际归并次数 / 次']):
    for p in range(1,4):
        ax.bar(x+(p-2)*0.25, vals(p,key), width=0.24, color=colors[p-1],label=f'Project {p}')
    ax.set_xticks(x, [str(n) for n in sizes]); ax.set_xlabel('输入整数数量 N'); ax.set_ylabel(ylabel)
    ax.grid(axis='y',alpha=.2); ax.legend(fontsize=7)
save(fig,'runs-merges.pdf')

fig, axes = plt.subplots(1,2,figsize=(7.1,2.65),layout='constrained')
for ax,key,title in zip(axes,['rounds','stages'],['归并轮数 / 轮','线程同步阶段总数 / 个']):
    values=vals(3,key)
    ax.bar(x,values,color=colors[2],width=.55)
    ax.set_xticks(x,[str(n) for n in sizes]); ax.set_xlabel('输入整数数量 N'); ax.set_ylabel(title)
    ax.set_ylim(0,max(values)*1.2); ax.grid(axis='y',alpha=.2)
    for i,v in enumerate(values): ax.text(i,v+max(values)*.025,str(int(v)),ha='center',fontsize=8)
save(fig,'rounds-stages.pdf')

fig, axes = plt.subplots(1,2,figsize=(7.1,2.8),layout='constrained')
reads=np.array([vals(p,'reads')[-1] for p in range(1,4)])
writes=np.array([vals(p,'writes')[-1] for p in range(1,4)])
axes[0].bar(np.arange(3),reads,color=colors[0],label='数据块读')
axes[0].bar(np.arange(3),writes,bottom=reads,color=colors[1],label='数据块写')
axes[0].set_xticks(range(3),['Project 1','Project 2','Project 3']); axes[0].set_ylabel('应用层调用次数 / 次')
axes[0].set_title('N = 50003'); axes[0].legend(fontsize=7); axes[0].grid(axis='y',alpha=.2)
axes[0].set_ylim(0,18500)
for i,v in enumerate(reads+writes): axes[0].text(i,v+250,str(int(v)),ha='center',fontsize=8)
for p in range(1,4): axes[1].plot(x,vals(p,'ms'),marker='o',color=colors[p-1],label=f'Project {p}')
axes[1].set_xticks(x,[str(n) for n in sizes]); axes[1].set_xlabel('输入整数数量 N')
axes[1].set_ylabel('单次排序时间 / ms（对数刻度）'); axes[1].set_yscale('log')
axes[1].legend(fontsize=7); axes[1].grid(alpha=.2,which='both')
save(fig,'io-time.pdf')
keys=['project','n','runs','merges','rounds','reads','writes','stages','ms']
(ROOT/'data/benchmark-table.tex').write_text('\n'.join(' & '.join(r[k] for k in keys)+r' \\' for r in rows)+'\n\\bottomrule\n',encoding='utf-8')
print('PASS: 3 vector figures (6 panels), 12 CSV rows, non-time columns match WSL')
