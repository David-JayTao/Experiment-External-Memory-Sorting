"""渲染所有PDF页面并生成逐页联系表；单页PNG用于放大检查。"""
from pathlib import Path
import pymupdf
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/review'; OUT.mkdir(parents=True,exist_ok=True)
doc=pymupdf.open(ROOT/'build/main.pdf')
# 页数减少后，只清理本脚本生成且已超出当前范围的旧预览。
for pattern,limit in [('page-*.png',len(doc)),('contact-*.jpg',(len(doc)+11)//12)]:
    for old in OUT.glob(pattern):
        number=old.stem.rsplit('-',1)[-1]
        if number.isdigit() and int(number)>limit:
            old.unlink()
for i,page in enumerate(doc):
    page.get_pixmap(matrix=pymupdf.Matrix(1.35,1.35),alpha=False).save(OUT/f'page-{i+1:03}.png')
for start in range(0,len(doc),12):
    sheet=Image.new('RGB',(1040,1510),'#d8dde0'); draw=ImageDraw.Draw(sheet)
    for offset in range(min(12,len(doc)-start)):
        n=start+offset+1
        im=Image.open(OUT/f'page-{n:03}.png'); im.thumbnail((250,350))
        x=(offset%4)*260+5; y=(offset//4)*500+30
        sheet.paste(im,(x,y)); draw.text((x,y-20),f'PDF page {n}',fill='black')
    sheet.save(OUT/f'contact-{start//12+1:02}.jpg',quality=90)
print(f'Rendered {len(doc)} pages and contact sheets to {OUT}')
