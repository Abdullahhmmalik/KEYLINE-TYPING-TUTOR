param([switch]$CoreOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) { throw 'GCC is required. See README.md.' }
$source = Join-Path $root 'src'
function Get-SourceHashes {
    @(Get-ChildItem -LiteralPath $source -File | Sort-Object Name | ForEach-Object {
        $_.Name + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }) -join "`n"
}
function Replace-Checked([string]$path, [string]$pattern, [string]$replacement) {
    $text = [IO.File]::ReadAllText($path)
    if ([regex]::Matches($text,$pattern).Count -ne 1) { throw "Expected one configuration edit: $pattern" }
    [IO.File]::WriteAllText($path,[regex]::Replace($text,$pattern,$replacement),(New-Object Text.UTF8Encoding($false)))
}
$before = Get-SourceHashes
$workspace = Join-Path $root ('build/config-variants-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $workspace -Force | Out-Null
$warnings = @('-std=c99','-Wall','-Wextra','-Wpedantic','-Wshadow','-Wstrict-prototypes','-O2')
$ray = Join-Path $root 'vendor/raylib-5.5_win64_mingw-w64'
try {
    foreach ($count in @(8,12)) {
        $variant = Join-Path $workspace "presets-$count"
        New-Item -ItemType Directory -Path $variant | Out-Null
        Copy-Item -LiteralPath $source -Destination $variant -Recurse
        $copy = Join-Path $variant 'src'
        $constants = [ordered]@{
            MIN_DURATION='3'; MAX_DURATION='900'; DEFAULT_DURATION='45'; DEFAULT_MIN_ACCURACY='82.5'
            RANK_MIN_SECONDS='12.5'; ACH_SPEED_ONE='37.5'; ACH_SPEED_TWO='63.25'; ACH_ACCURACY='96.5'; ACH_TEST_COUNT='3'
        }
        if ($count -eq 12) { $constants.RANK_MIN_SECONDS='13'; $constants.ACH_SPEED_TWO='64' }
        foreach ($name in $constants.Keys) {
            Replace-Checked (Join-Path $copy 'config.h') "(?m)^#define $name [^\r\n]+" ("#define $name " + $constants[$name])
        }
        $presets = '15, 30, 45, 60, 90, 120, 180, 240'
        if ($count -eq 12) { $presets += ', 300, 420, 600, 900' }
        Replace-Checked (Join-Path $copy 'config.c') 'const int DURATION_PRESETS\[\] = \{[^}]*\};' ("const int DURATION_PRESETS[] = {$presets};")
        $core = @('config.c','stats.c','typing.c','data.c','achievements.c') | ForEach-Object { Join-Path $copy $_ }
        $coreExe = Join-Path $variant 'core.exe'
        & gcc @warnings "-I$copy" @core (Join-Path $root 'tests/test_config_variants.c') '-o' $coreExe '-lm'
        if ($LASTEXITCODE -ne 0) { throw "Core variant compilation failed ($count presets)." }
        & $coreExe (Join-Path $variant 'data') $count
        if ($LASTEXITCODE -ne 0) { throw "Core variant checks failed ($count presets)." }
        if (-not $CoreOnly) {
            if (-not (Test-Path -LiteralPath "$ray/lib/libraylib.a")) { throw 'Run build.bat first to obtain raylib.' }
            $guiExe = Join-Path $variant 'gui.exe'
            $gui = @('app.c','ui.c','desktop.c') | ForEach-Object { Join-Path $copy $_ }
            & gcc @warnings "-I$copy" "-I$ray/include" @core @gui (Join-Path $root 'tests/test_presets_gui.c') "$ray/lib/libraylib.a" '-o' $guiExe '-static' '-static-libgcc' '-lopengl32' '-lgdi32' '-lwinmm' '-luser32' '-lm'
            if ($LASTEXITCODE -ne 0) { throw "GUI variant compilation failed ($count presets)." }
            $images = Join-Path $variant 'images'
            New-Item -ItemType Directory -Path $images | Out-Null
            & $guiExe (Join-Path $variant 'gui-data') $images
            if ($LASTEXITCODE -ne 0) { throw "GUI variant checks failed ($count presets). Inspect $images." }
        }
    }
}
finally {
    if ((Get-SourceHashes) -ne $before) { throw 'Production source hashes changed during variant verification.' }
    Write-Host 'Production source SHA-256 hashes preserved.'
}
Write-Host "Configuration variants passed. Copies and fixtures: $workspace"
