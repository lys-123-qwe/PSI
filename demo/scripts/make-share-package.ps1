param(
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$demoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$repoRoot = Split-Path -Parent $demoRoot
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot "outputs"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$stageRoot = Join-Path ([IO.Path]::GetTempPath()) ("psi-demo-stage-" + [guid]::NewGuid().ToString("N"))
$stage = Join-Path $stageRoot "PSI-Demo"
New-Item -ItemType Directory -Path $stage | Out-Null

$rootItems = @(
    "start.cmd",
    "start.ps1",
    "start.sh",
    "start-backend.sh",
    "start-demo.cmd",
    "connect-server.cmd",
    "backend.env.example",
    "update-from-github.ps1",
    "update-from-github.cmd",
    "README.md",
    "SHARE.md",
    "remote-backend.example.txt",
    "BACKENDS.md",
    "PAPER_PATH.md",
    "BINARIES.md",
    "server",
    "web",
    "data",
    "thirdparty",
    "patches",
    "scripts"
)

function Copy-PackageItem([string]$RelativePath) {
    $source = Join-Path $demoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $source)) {
        Write-Warning "跳过不存在的文件：$RelativePath"
        return
    }
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        $dest = Join-Path $stage $RelativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        Copy-Item -LiteralPath $source -Destination $dest
        return
    }
    Get-ChildItem -LiteralPath $source -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($demoRoot.Length + 1)
        if ($rel -match '(^|\\)(__pycache__|out|bin)(\\|$)') { return }
        if ($_.Name -like "*.pyc" -or $_.Name -like "*.pyo") { return }
        if ($_.Name -eq "remote-backend.txt") { return }
        $dest = Join-Path $stage $rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $dest
    }
}

foreach ($item in $rootItems) {
    Copy-PackageItem $item
}

$zip = Join-Path $OutDir ("PSI-Demo-Windows-" + $stamp + ".zip")
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -LiteralPath $stage -DestinationPath $zip -CompressionLevel Optimal

$sizeMb = [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 2)
Write-Host ""
Write-Host "[package] 已生成：$zip"
Write-Host "[package] 大小：$sizeMb MB"
Write-Host "[package] 对方解压后双击 start-demo.cmd 即可；要用真实模式则双击 connect-server.cmd。"

$resolvedStage = (Resolve-Path -LiteralPath $stageRoot).Path
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
if ($resolvedStage.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
    (Split-Path -Leaf $resolvedStage) -like "psi-demo-stage-*") {
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
