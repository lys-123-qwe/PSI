param(
    [string]$Remote = "origin",
    [string]$Branch = "main",
    [switch]$NoStart
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "未找到 git。请先安装 Git for Windows：https://git-scm.com/download/win"
}

Write-Host "[update] 仓库：$repo"
Write-Host "[update] 拉取 $Remote/$Branch ..."
& git pull --ff-only $Remote $Branch
if ($LASTEXITCODE -ne 0) {
    throw "git pull 失败。若本地有未提交改动，请先在 IDEA 里提交/暂存，或手动处理冲突后重试。"
}

Write-Host "[update] 代码已更新。"
if ($NoStart) {
    Write-Host "[update] 未启动服务（-NoStart）。"
    exit 0
}

Write-Host "[update] 启动本地 Demo ..."
& (Join-Path $PSScriptRoot "start.ps1")
