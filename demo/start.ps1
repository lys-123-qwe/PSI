$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$candidates = @(
    "C:\Users\Lenovo\AppData\Local\Programs\Python\Python312\python.exe",
    "python"
)
$python = $null
foreach ($candidate in $candidates) {
    try {
        & $candidate -c "import sys; print(sys.version_info[:2])" | Out-Null
        $python = $candidate
        break
    } catch { }
}
if (-not $python) { throw "Python 3.10+ not found" }

Start-Process "http://127.0.0.1:8787" | Out-Null
& $python "server/app.py" --port 8787
