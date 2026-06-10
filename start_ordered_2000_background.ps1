$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$resultRoot = Join-Path $scriptDir "results_ablation_g2000"
$stdout = Join-Path $resultRoot "stdout.log"
$stderr = Join-Path $resultRoot "stderr.log"
$runner = Join-Path $scriptDir "run_ordered_2000_experiments.ps1"

New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$process = Start-Process `
    -FilePath "powershell" `
    -ArgumentList @("-ExecutionPolicy", "Bypass", "-File", $runner, "-Generations", "2000", "-Population", "100", "-CheckpointInterval", "1") `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -WindowStyle Hidden `
    -PassThru

Write-Output "Started ordered ablation 2000 run. PID=$($process.Id)"
Write-Output "stdout=$stdout"
Write-Output "stderr=$stderr"
