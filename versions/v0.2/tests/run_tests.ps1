param([string]$Compiler = 'gcc', [string]$Node = 'node')
$ErrorActionPreference = 'Stop'
$versionRoot = Split-Path $PSScriptRoot -Parent
$testBuild = Join-Path ([IO.Path]::GetTempPath()) 'door-lock-v02-tests'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null

# Test the actual hardware-control C with a mock register array. Only Cosmic
# annotations and the entry-point name change; CPU sleep instructions are mocked.
$mainText = [IO.File]::ReadAllText((Join-Path $versionRoot 'main.c'))
$mainText = $mainText.Replace('@far @interrupt ', '').Replace('void main(void)', 'void firmware_main(void)')
[IO.File]::WriteAllText((Join-Path $testBuild 'main_host.c'), $mainText)
foreach ($name in @('test_knock', 'test_hardware', 'test_force_high')) {
    $exe = Join-Path $testBuild ($name + '.exe')
    & $Compiler -Wall -Werror "-I$versionRoot" "-I$testBuild" (Join-Path $PSScriptRoot ($name + '.c')) -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $name" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $name" }
}
& $Node (Join-Path $PSScriptRoot 'check_web.js')
if ($LASTEXITCODE -ne 0) { throw 'Web validation failed' }
