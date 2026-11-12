param([switch]$Render)
$ErrorActionPreference = 'Stop'
$ReportRoot = Split-Path $PSScriptRoot -Parent
$XeLaTeX = (Get-Command xelatex -ErrorAction SilentlyContinue).Source
if (-not $XeLaTeX) { $XeLaTeX = 'C:\texlive\2026\bin\windows\xelatex.exe' }
if (-not (Test-Path -LiteralPath $XeLaTeX)) { throw 'XeLaTeX not found. Install TeX Live or set PATH.' }
$Python = Join-Path $env:USERPROFILE 'miniforge3\python.exe'
if (-not (Test-Path -LiteralPath $Python)) { $Python = (Get-Command python).Source }
Push-Location $ReportRoot
try {
    New-Item -ItemType Directory -Force -Path build | Out-Null
    for ($ReportPass = 1; $ReportPass -le 3; $ReportPass++) {
        & $XeLaTeX -interaction=nonstopmode -halt-on-error -output-directory=build main.tex > "build/pass-$ReportPass.txt"
        if ($LASTEXITCODE -ne 0) { throw "XeLaTeX failed on pass $ReportPass; see build/main.log" }
    }
    & $Python scripts/verify_report.py
    if ($LASTEXITCODE -ne 0) { throw 'Report verification failed.' }
    if ($Render) {
        & $Python scripts/render_review.py
        if ($LASTEXITCODE -ne 0) { throw 'Rendering failed.' }
    }
    Copy-Item -LiteralPath build/main.pdf -Destination '外存排序课程实践实验报告_刘酝潇_09024211.pdf' -Force
} finally { Pop-Location }
