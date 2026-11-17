param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Url = @(),
    [string]$Primary = "",
    [string]$Backup = "",
    [int]$Port = 8787,
    [switch]$CheckOnly,
    [switch]$NoStart,
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"
$demoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $demoRoot

function Write-Step([string]$Message) {
    Write-Host "[connect] $Message"
}

if ((-not $Url -or $Url.Count -eq 0) -and -not $Primary) {
    Write-Host ""
    Write-Host "请输入真实后端地址（只填 IP 也可以，脚本会自动补 http://）："
    Write-Host "  主实例：自己的 Linux 实例（同一台导师服务器可用 8787）"
    Write-Host "  备实例：对方的 Linux 实例（同一台导师服务器可用 8788，可留空）"
    $Primary = Read-Host "主后端地址"
    $Backup = Read-Host "备用后端地址（可留空）"
}

$urls = New-Object System.Collections.Generic.List[string]
$rawParts = @()
if ($Primary) { $rawParts += $Primary }
if ($Backup) { $rawParts += $Backup }
if ($Url) { $rawParts += $Url }
foreach ($part in (($rawParts -join " ") -split "[\s,;]+")) {
    $text = $part.Trim().Trim([char]0xFEFF).Trim('"').Trim("'").TrimEnd("/")
    if (-not $text) { continue }
    if ($text -notmatch '^[a-zA-Z][a-zA-Z0-9+.-]*://') {
        $text = "http://$text"
    }
    $uri = $null
    if (-not [Uri]::TryCreate($text, [UriKind]::Absolute, [ref]$uri)) {
        Write-Warning "忽略格式不正确的地址：$part"
        continue
    }
    if ($uri.Scheme -notin @("http", "https")) {
        Write-Warning "忽略非 http/https 地址：$part"
        continue
    }
    if (-not $urls.Contains($text)) {
        $urls.Add($text)
    }
}
if ($urls.Count -eq 0) {
    throw "没有有效的服务器地址。"
}

$ready = New-Object System.Collections.Generic.List[string]
for ($i = 0; $i -lt $urls.Count; $i++) {
    $target = $urls[$i]
    $role = if ($i -eq 0) { "主" } else { "备" }
    Write-Step ("检查 {0}/{1} [{2}] {3}/api/health ..." -f ($i + 1), $urls.Count, $role, $target)
    $health = $null
    try {
        $response = Invoke-WebRequest -Uri "$target/api/health" -TimeoutSec 6 -UseBasicParsing
        if ($response.StatusCode -eq 200) {
            $health = $response.Content | ConvertFrom-Json
        }
    } catch {
        Write-Warning ("{0} 不可达：{1}" -f $target, $_.Exception.Message)
        continue
    }
    if (-not $health.ok) {
        Write-Warning ("{0} 健康检查返回 ok=false" -f $target)
        continue
    }
    if ($health.backend -and $health.backend.exists) {
        Write-Host ("[connect] {0} 在线，真实后端 {1}，版本 {2}" -f $target, $health.backend.mode, $health.version)
        $ready.Add($target)
    } else {
        Write-Warning ("{0} 在线，但它没有配置真实后端" -f $target)
    }
}

if ($ready.Count -eq 0) {
    Write-Warning "所有地址都没有可用的真实后端；本机只能使用演示模式。地址仍会保存，稍后可再试。"
} else {
    Write-Step ("可用后端 {0}/{1}；优先使用 {2}" -f $ready.Count, $urls.Count, $ready[0])
}

$configPath = Join-Path $demoRoot "remote-backend.txt"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($configPath, (($urls -join "`r`n") + "`r`n"), $utf8NoBom)
Write-Step ("已写入 {0}（{1} 个地址，按优先级排列）" -f $configPath, $urls.Count)

if ($CheckOnly) {
    Write-Host "[connect] 仅检测，未启动服务（-CheckOnly）。"
    exit 0
}

if ($NoStart) {
    Write-Host "[connect] 已完成配置，未启动服务（-NoStart）。"
    exit 0
}

$env:PSI_REMOTE = ($urls -join ";")
$env:PSI_PUBLIC_URL = if ($ready.Count -gt 0) { $ready[0] } else { $urls[0] }
$env:PORT = [string]$Port
if ($NoBrowser) {
    $env:PSI_NO_BROWSER = "1"
}

Write-Step "启动本机 Demo：http://127.0.0.1:$Port"
Write-Step "主后端不可达时会自动尝试下一个；进度和取消会绑定到实际运行的那台。"
& (Join-Path $demoRoot "start.ps1")
