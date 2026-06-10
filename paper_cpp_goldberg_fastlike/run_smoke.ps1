$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$python = Join-Path $projectRoot ".venv311\python.exe"
$data = Join-Path $projectRoot "data_100200400.xlsx"
$instanceDir = Join-Path $scriptDir "instances\data_100200400"
$outDir = Join-Path $scriptDir "results_smoke\data_100200400"
$exe = Join-Path $scriptDir "paper_ga_fastlike.exe"

& $python (Join-Path $scriptDir "export_instance.py") --data "$data" --out-dir "$instanceDir"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $scriptDir "build.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $exe --instance-dir "$instanceDir" --out-dir "$outDir" --population 4 --generations 5 --elitism-interval 50 --mutation-rate 0.03 --seed 42 --checkpoint-interval 1
exit $LASTEXITCODE
