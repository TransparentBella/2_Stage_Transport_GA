$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $scriptDir "results_formal\data_100200400"
$stdout = Join-Path $outDir "stdout.log"
$stderr = Join-Path $outDir "stderr.log"
$runner = Join-Path $scriptDir "run_formal.ps1"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$process = Start-Process `
    -FilePath "powershell" `
    -ArgumentList @("-ExecutionPolicy", "Bypass", "-File", $runner) `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -WindowStyle Hidden `
    -PassThru

Write-Output "Started Goldberg formal run. PID=$($process.Id)"
Write-Output "stdout=$stdout"
Write-Output "stderr=$stderr"
