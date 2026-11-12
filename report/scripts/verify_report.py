"""校验源码快照、PDF页数/文字/边界和LaTeX日志；渲染后另需人工视觉检查。"""
from pathlib import Path
import hashlib
import json
import re
import pymupdf

ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'code_appendix/manifest.json').read_text(encoding='utf-8'))
assert sum(e['cpp'] for e in manifest['files'])==17
listing=(ROOT/'code_appendix/listings.tex').read_text(encoding='utf-8')
for entry in manifest['files']:
    p=ROOT/'code_appendix'/entry['path']
    assert hashlib.sha256(p.read_bytes()).hexdigest()==entry['sha256'],p
    assert f'\\VerbatimInput{{code_appendix/{entry["path"]}}}' in listing,p
log=(ROOT/'build/main.log').read_text(encoding='utf-8',errors='replace')
bad=[line for line in log.splitlines() if re.search(r'^!|Missing character:|undefined references|Reference .* undefined|Overfull \\[hv]box',line)]
doc=pymupdf.open(ROOT/'build/main.pdf')
texts=[p.get_text() for p in doc]
empty=[i+1 for i,t in enumerate(texts) if len(t.strip())<15]
assert not empty,empty
appendix=next(i+1 for i,t in enumerate(texts) if '完整源代码' in t and '本章由脚本' in t)
body=next(i+1 for i,t in enumerate(texts) if '问题描述与需求分析' in t and '若把全部输入装入' in re.sub(r'\s+', '', t))
outside=[]
for i,p in enumerate(doc):
    for word in p.get_text('words'):
        if word[0]<15 or word[2]>p.rect.width-15 or word[1]<8 or word[3]>p.rect.height-8:
            outside.append([i+1,word[:5]])
report={'pages':len(doc),'front_pages':body-1,'body_pages':appendix-body,
        'appendix_start_pdf_page':appendix,'appendix_pages':len(doc)-appendix+1,
        'cpp_files':17,'source_commit':manifest['commit'],'latex_issues':bad,'edge_issues':outside}
(ROOT/'build/verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
assert not bad,'Fix LaTeX warnings before delivery'
assert not outside,'Inspect text near page edges'
