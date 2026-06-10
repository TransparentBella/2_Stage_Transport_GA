$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $scriptDir "paper_ga_goldberg.cpp"
$exe = Join-Path $scriptDir "paper_ga_fastlike.exe"

g++ -O3 -std=c++17 "$source" -o "$exe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Output $exe
