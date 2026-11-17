$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$env:PYTHONUNBUFFERED = "1"
$port = if ($env:PORT) { [int]$env:PORT } else { 8787 }
$bindHost = if ($env:HOST) { $env:HOST } else { "127.0.0.1" }

# Optional remote real backends.  Keep the URLs in the git-ignored
# remote-backend.txt (copy remote-backend.example.txt) instead of hard-coding a
# lab server in the repository.  One URL per line, priority order.
$remoteFile = Join-Path $root "remote-backend.txt"
if (-not $env:PSI_REMOTE -and (Test-Path $remoteFile)) {
    $remotes = @(Get-Content $remoteFile -Encoding UTF8 |
        ForEach-Object { ($_ -split "#", 2)[0].Trim().Trim([char]0xFEFF) } |
        Where-Object { $_ })
    if ($remotes.Count -gt 0) { $env:PSI_REMOTE = ($remotes -join ";") }
}
if ($env:PSI_REMOTE -and -not $env:PSI_PUBLIC_URL) {
    $env:PSI_PUBLIC_URL = (($env:PSI_REMOTE -split "[;,\s]+")[0]).Trim().Trim([char]0xFEFF)
}

$candidates = @()
if ($env:PYTHON) { $candidates += ,@($env:PYTHON) }
if (Get-Command py -ErrorAction SilentlyContinue) { $candidates += ,@("py", "-3") }
if ($env:LOCALAPPDATA) {
    $candidates += ,@((Join-Path $env:LOCALAPPDATA "Programs\Python\Python314\python.exe"))
    $candidates += ,@((Join-Path $env:LOCALAPPDATA "Programs\Python\Python313\python.exe"))
    $candidates += ,@((Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"))
    $candidates += ,@((Join-Path $env:LOCALAPPDATA "Programs\Python\Python311\python.exe"))
    $candidates += ,@((Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"))
}
$candidates += ,@("python3")
$candidates += ,@("python")

$python = $null
foreach ($candidate in $candidates) {
    if (-not $candidate -or -not $candidate[0]) { continue }
    $exe = $candidate[0]
    $prefix = if ($candidate.Length -gt 1) { $candidate[1..($candidate.Length - 1)] } else { @() }
    try {
        & $exe @prefix -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)" 2>$null
        if ($LASTEXITCODE -eq 0) {
            $python = $candidate
            break
        }
    } catch { }
}
if (-not $python) {
    throw "未找到 Python 3.8+。请安装 Python 3.10 或更高版本（推荐）：https://www.python.org/downloads/ （安装时勾选 Add Python to PATH），或设置 PYTHON 环境变量指向 python.exe。"
}

Write-Host "PSI Lab Demo -> http://127.0.0.1:$port"
Write-Host "  demo: /   real data: /real"
Write-Host "  python: $($python -join ' ')"
if ($env:PSI_REMOTE) {
    Write-Host "  remote backend: $env:PSI_REMOTE"
} else {
    Write-Host "  remote backend: not configured (demo mode only)"
}
if (-not $env:PSI_NO_BROWSER) {
    Start-Process "http://127.0.0.1:$port" | Out-Null
}
$pythonExe = $python[0]
$pythonPrefix = if ($python.Length -gt 1) { $python[1..($python.Length - 1)] } else { @() }
& $pythonExe @pythonPrefix -u "server/app.py" --host $bindHost --port $port
