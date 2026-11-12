"""在仓库根或任意目录执行；只对已提交且干净的正式源码建立快照。"""
from pathlib import Path
import hashlib
import json
import subprocess

REPORT = Path(__file__).resolve().parents[1]
ROOT = REPORT.parent
OUT = REPORT / 'code_appendix'

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

def norm(data):
    return data.replace(b'\r\n', b'\n')

commit = git('rev-parse', 'HEAD').decode().strip()
names = ['main.cpp']
for folder in ('src', 'tests'):
    names += sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / folder).iterdir()
                    if p.suffix in ('.h', '.cpp'))
assert len(names) == 17, f'源码范围变化，请审查：{len(names)}个文件'
entries = []
listing = []
for name in names + ['CMakeLists.txt']:
    source = (ROOT / name).read_bytes()
    assert norm(source) == norm(git('show', f'{commit}:{name}')), f'未提交修改：{name}'
    dest = OUT / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(source)
    entries.append({'path': name, 'bytes': len(source), 'lines': len(source.splitlines()),
                    'sha256': hashlib.sha256(source).hexdigest(), 'cpp': name in names})
    title = name.replace('_', r'\_')
    listing += [r'\Needspace{8\baselineskip}', r'\section{' + title + '}',
                r'\VerbatimInput{code_appendix/' + name + '}']
OUT.mkdir(exist_ok=True)
(OUT / 'manifest.json').write_text(json.dumps({'commit': commit, 'cpp_files': 17,
    'files': entries}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
(OUT / 'listings.tex').write_text('\n'.join(listing) + '\n', encoding='utf-8')
(OUT / 'version.tex').write_text('快照对应Git提交：\\par\\noindent{\\small\\texttt{' + commit +
    '}}\\par\n正式C++文件共17个，另附CMakeLists.txt作为构建配置。\n', encoding='utf-8')
print(f'PASS snapshot: {commit}, 17 C++ files + CMakeLists.txt')
