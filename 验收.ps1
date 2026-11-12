[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('week3', 'project1', 'project2', 'project3', 'test', 'benchmark', 'ctest', 'all', 'build')]
    [string]$Mode = 'all',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateRange(2, 1024)]
    [int]$K = 3,
    [switch]$Large
)

# UTF-8 BOM 保证 Windows PowerShell 5.1 正确读取本脚本的中文。
# 所有路径以脚本位置为准，调用者不必先进入仓库；不修改 PATH 或执行策略。
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root 'build\windows'
$runDir = Join-Path $buildDir "run\$Configuration"
$preset = 'windows-' + $Configuration.ToLowerInvariant()
$exe = Join-Path $buildDir "$Configuration\external_sort.exe"
$results = New-Object System.Collections.Generic.List[object]

function Find-CMake {
    $found = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }
    $candidates = @(
        (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
        (Join-Path $env:USERPROFILE 'miniforge3\Scripts\cmake.exe'),
        (Join-Path $env:USERPROFILE 'miniconda3\Scripts\cmake.exe')
    )
    if ($env:CONDA_PREFIX) { $candidates += Join-Path $env:CONDA_PREFIX 'Scripts\cmake.exe' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) { $candidates += Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw '未找到 Windows CMake 3.21+。请按 docs/Windows环境与验收指南.md 安装 CMake。'
}

function Invoke-Checked([string]$Tool, [string[]]$ToolArgs) {
    & $Tool @ToolArgs
    if ($LASTEXITCODE -ne 0) { throw "命令失败（退出码 $LASTEXITCODE）：$Tool $($ToolArgs -join ' ')" }
}

function Invoke-Item([string]$Title, [string[]]$ProgramArgs, [switch]$CTest) {
    Write-Host "`n $Title " -ForegroundColor Cyan
    $passed = $false
    Push-Location -LiteralPath $runDir
    try {
        if ($CTest) {
            # CTest 在构建目录中运行，演示文件在 run/配置 中；两者互不覆盖。
            Invoke-Checked $script:ctest @('--test-dir', $buildDir, '-C', $Configuration, '--output-on-failure')
        } else {
            Invoke-Checked $exe $ProgramArgs
        }
        $passed = $true
    } catch {
        Write-Host $_.Exception.Message -ForegroundColor Red
    } finally {
        Pop-Location
    }
    $status = if ($passed) { 'PASS' } else { 'FAIL' }
    $results.Add([pscustomobject]@{ Item = $Title; Status = $status })
    Write-Host "[$status] $Title"
}

$exitCode = 0
Push-Location -LiteralPath $root
try {
    if ($env:OS -ne 'Windows_NT') { throw '本脚本用于 Windows 原生构建；Linux/WSL 请使用 README 中的命令。' }
    if ($Large -and $Mode -ne 'project3') { throw '-Large 仅适用于 project3；all 已包含大规模验收。' }
    $cmake = Find-CMake
    $script:ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    Write-Host "`n Windows 原生配置 / $Configuration " -ForegroundColor Cyan
    Write-Host "CMake：$cmake"
    # 首次配置显式选择实际安装的 VS；版本名来自 CMake 能力列表，不写死 VS 年份。
    $configureArgs = @('--preset', 'windows')
    if (-not (Test-Path -LiteralPath (Join-Path $buildDir 'CMakeCache.txt'))) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw '未找到 Visual Studio Installer；请先安装 MSVC Build Tools 和 Windows SDK。' }
        $instances = @((& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | Out-String) | ConvertFrom-Json)
        if ($instances.Count -eq 0) { throw '未找到带 x64/x86 C++ 工具的 Visual Studio / Build Tools。' }
        $vs = $instances[0]
        $major = ([version]$vs.installationVersion).Major
        $capabilities = (& $cmake -E capabilities | Out-String) | ConvertFrom-Json
        $generator = @($capabilities.generators | Where-Object { $_.name -like "Visual Studio $major *" })
        if ($generator.Count -ne 1) { throw "此 CMake 不支持已安装的 Visual Studio $major；请升级 CMake。" }
        $configureArgs += @('-G', $generator[0].name, "-DCMAKE_GENERATOR_INSTANCE=$($vs.installationPath)")
    }
    Invoke-Checked $cmake $configureArgs
    $cache = Get-Content -LiteralPath (Join-Path $buildDir 'CMakeCache.txt')
    if (-not ($cache -match '^CMAKE_GENERATOR:INTERNAL=Visual Studio ')) {
        throw 'build/windows 必须使用 Visual Studio 多配置生成器；请将旧目录改名后重新运行。'
    }
    Write-Host "`n 增量编译 " -ForegroundColor Cyan
    Invoke-Checked $cmake @('--build', '--preset', $preset)
    if (-not (Test-Path -LiteralPath $exe)) { throw "编译未生成预期程序：$exe" }
    New-Item -ItemType Directory -Force -Path $runDir | Out-Null
    Write-Host "程序：$exe"
    Write-Host "运行目录：$runDir"

    if ($Mode -eq 'all') {
        Invoke-Item 'Week 3 文件与缓冲区' @('week3')
        Invoke-Item 'Project 1 二路外排序' @('project1')
        Invoke-Item 'Project 2 置换选择' @('project2')
        foreach ($ways in 3, 2, 4) { Invoke-Item "Project 3 K 路归并 k=$ways" @('project3', '--k', "$ways") }
        Invoke-Item 'Project 3 大规模 N=50003' @('project3', '--size', '50003', '--memory', '256', '--block', '64', '--k', '4')
        Invoke-Item 'Week 8 全部正确性测试' @('test')
        Invoke-Item 'Benchmark' @('benchmark')
        Invoke-Item '全部 CTest' @() -CTest
    } elseif ($Mode -eq 'ctest') {
        Invoke-Item '全部 CTest' @() -CTest
    } elseif ($Mode -eq 'project3') {
        $programArgs = @('project3', '--k', "$K")
        if ($Large) { $programArgs += @('--size', '50003', '--memory', '256', '--block', '64') }
        Invoke-Item "Project 3 k=$K（大规模：$Large）" $programArgs
    } elseif ($Mode -ne 'build') {
        Invoke-Item $Mode @($Mode)
    }
    if ($results.Count -gt 0) {
        Write-Host "`n 实际验收汇总 / $Configuration " -ForegroundColor Cyan
        $results | Format-Table -AutoSize | Out-Host
        if (@($results | Where-Object Status -eq 'FAIL').Count -gt 0) { $exitCode = 1 }
    }
} catch {
    Write-Host "[FAIL] $($_.Exception.Message)" -ForegroundColor Red
    $exitCode = 1
} finally {
    Pop-Location
}
exit $exitCode
