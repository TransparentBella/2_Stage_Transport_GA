param(
    [int]$Generations = 51,
    [int]$Population = 100,
    [int]$CheckpointInterval = 1
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$python = Join-Path $projectRoot ".venv311\python.exe"
$data = Join-Path $projectRoot "data_100200400.xlsx"
$instanceDir = Join-Path $scriptDir "instances\data_100200400"
$exe = Join-Path $scriptDir "paper_ga_ablation.exe"
$resultRoot = Join-Path $scriptDir "results_ablation_g$Generations"

& $python (Join-Path $scriptDir "export_instance.py") --data "$data" --out-dir "$instanceDir"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $scriptDir "build.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$runs = @(
    @{
        Name = "top3_full_ls"
        Extra = @("--elitism-top-k", "3")
    },
    @{
        Name = "top3_single_pass_ls"
        Extra = @("--elitism-top-k", "3", "--ls-single-pass")
    },
    @{
        Name = "final_only_top3_single_pass"
        Extra = @("--elitism-top-k", "3", "--ls-single-pass", "--elitism-final-only")
    }
)

foreach ($run in $runs) {
    $outDir = Join-Path $resultRoot $run.Name
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $args = @(
        "--instance-dir", "$instanceDir",
        "--out-dir", "$outDir",
        "--population", "$Population",
        "--generations", "$Generations",
        "--elitism-interval", "50",
        "--mutation-rate", "0.03",
        "--seed", "42",
        "--checkpoint-interval", "$CheckpointInterval"
    ) + $run.Extra
    & $exe @args
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $python (Join-Path $scriptDir "summarize_ablation.py") --result-root "$resultRoot"
exit $LASTEXITCODE
