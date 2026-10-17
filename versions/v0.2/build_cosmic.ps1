param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$Toolchain = 'E:\stm8l-tools\cosmic\CXSTM8'
)
$ErrorActionPreference = 'Stop'
$compilerExe = Join-Path $Toolchain 'cxstm8.exe'
if (!(Test-Path -LiteralPath $compilerExe)) { throw "Cosmic compiler not found: $compilerExe" }
$originalPath = $env:PATH
Push-Location $PSScriptRoot
try {
    $env:PATH = "$Toolchain;$originalPath"
    New-Item -ItemType Directory -Force -Path $Configuration | Out-Null
    $compileOptions = @('+mods0', '-pp', "-i$Toolchain\Hstm8", "-cl$Configuration\", "-co$Configuration\")
    if ($Configuration -eq 'Debug') { $compileOptions = @('+debug', '-pxp', '-no', '-l') + $compileOptions }
    foreach ($source in @('main.c', 'stm8_interrupt_vector.c')) {
        & $compilerExe @compileOptions $source
        if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    }
    & (Join-Path $Toolchain 'clnk.exe') -m "$Configuration\doorup.map" "-l$Toolchain\Lib" -o "$Configuration\doorup.sm8" "$Configuration\doorup.lkf"
    if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
    & (Join-Path $Toolchain 'cvdwarf.exe') "$Configuration\doorup.sm8"
    if ($LASTEXITCODE -ne 0) { throw 'ELF conversion failed' }
    & (Join-Path $Toolchain 'chex.exe') -o "$Configuration\doorup.s19" "$Configuration\doorup.sm8"
    if ($LASTEXITCODE -ne 0) { throw 'S19 conversion failed' }
    Write-Output "Built: $PSScriptRoot\$Configuration\doorup.s19"
} finally {
    $env:PATH = $originalPath
    Pop-Location
}
