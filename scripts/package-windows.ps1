param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,
    [string]$QtBin = 'C:\Qt\5.15.2\msvc2019_64\bin'
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($env:VCINSTALLDIR)) {
    throw '请在 MSVC x64 开发者命令行中运行，以便部署编译器运行库'
}
$projectDir = Split-Path -Parent $PSScriptRoot
$versionText = Get-Content -LiteralPath (Join-Path $projectDir 'CMakeLists.txt') -Raw
$versionMatch = [regex]::Match($versionText, 'project\(EasyBoard VERSION (\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) {
    throw '无法从 CMakeLists.txt 读取 EasyBoard 版本'
}
$version = $versionMatch.Groups[1].Value
$buildPath = (Resolve-Path -LiteralPath $BuildDir).Path
$deploy = Join-Path $QtBin 'windeployqt.exe'
if (-not (Test-Path -LiteralPath $deploy -PathType Leaf)) {
    throw "找不到 windeployqt.exe：$deploy"
}

$binaryDir = $buildPath
if (Test-Path -LiteralPath (Join-Path $buildPath 'Release\EasyBoard.exe')) {
    $binaryDir = Join-Path $buildPath 'Release'
}
$app = Join-Path $binaryDir 'EasyBoard.exe'
$renderer = Join-Path $binaryDir 'EasyBoardPdfRenderer.exe'
foreach ($path in @($app, $renderer)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "缺少 Release 构建产物：$path"
    }
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDir).Path
$packageName = "EasyBoard-$version-windows-msvc2019-x64"
$packageDir = Join-Path $outputPath $packageName
$zipPath = Join-Path $outputPath "$packageName.zip"
if ((Test-Path -LiteralPath $packageDir) -or (Test-Path -LiteralPath $zipPath)) {
    throw "发布包已存在，请指定空的输出目录：$packageDir"
}

New-Item -ItemType Directory -Path $packageDir | Out-Null
Copy-Item -LiteralPath $app -Destination $packageDir
Copy-Item -LiteralPath $renderer -Destination $packageDir
& $deploy --release --compiler-runtime --dir $packageDir (Join-Path $packageDir 'EasyBoard.exe')
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt 执行失败：$LASTEXITCODE"
}

$redistRoot = Join-Path $env:VCINSTALLDIR 'Redist\MSVC'
if (-not (Test-Path -LiteralPath $redistRoot -PathType Container)) {
    throw "找不到 MSVC 运行库目录：$redistRoot"
}
$crtDirectory = Get-ChildItem -LiteralPath $redistRoot -Directory |
    Sort-Object Name -Descending |
    ForEach-Object {
        Get-ChildItem -LiteralPath (Join-Path $_.FullName 'x64') `
            -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue
    } | Select-Object -First 1
if (-not $crtDirectory) {
    throw '找不到 x64 MSVC 运行库'
}
Get-ChildItem -LiteralPath $crtDirectory.FullName -File -Filter '*.dll' |
    ForEach-Object {
        $destination = Join-Path $packageDir $_.Name
        if (-not (Test-Path -LiteralPath $destination)) {
            Copy-Item -LiteralPath $_.FullName -Destination $destination
        }
    }

$webProcess = Get-ChildItem -LiteralPath $packageDir -Recurse -Filter 'QtWebEngineProcess.exe' -File
$webResources = Get-ChildItem -LiteralPath $packageDir -Recurse -Filter 'qtwebengine_resources.pak' -File
if (-not $webProcess -or -not $webResources) {
    throw 'Qt WebEngine 运行文件不完整，请检查 windeployqt 输出'
}
if (-not (Test-Path -LiteralPath (Join-Path $packageDir 'Qt5Core.dll'))) {
    throw 'Qt 运行库未复制到发布包'
}
foreach ($runtime in @('vcruntime140.dll', 'msvcp140.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $packageDir $runtime))) {
        throw "MSVC 运行库未复制到发布包：$runtime"
    }
}

$checksums = @('EasyBoard.exe', 'EasyBoardPdfRenderer.exe') | ForEach-Object {
    $hash = Get-FileHash -LiteralPath (Join-Path $packageDir $_) -Algorithm SHA256
    "$($hash.Hash)  $_"
}
Set-Content -LiteralPath (Join-Path $packageDir 'SHA256SUMS.txt') `
    -Value $checksums -Encoding Ascii
Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath
Write-Output $zipPath
