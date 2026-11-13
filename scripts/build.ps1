param([switch]$TestOnly, [switch]$Run, [string]$DataDir = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
$compiler = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $compiler) { throw 'Install MSYS2 UCRT64 GCC and add C:\msys64\ucrt64\bin to PATH. See README.md.' }
New-Item -ItemType Directory -Force -Path 'build' | Out-Null
$core = @('src/config.c','src/stats.c','src/typing.c','src/data.c','src/achievements.c')
$warnings = @('-std=c99','-Wall','-Wextra','-Wpedantic','-Wshadow','-Wstrict-prototypes')
& gcc @warnings '-O2' '-Isrc' @core 'tests/test_core.c' '-o' 'build/keyline_tests.exe' '-lm'
if ($LASTEXITCODE -ne 0) { throw 'Core test compilation failed.' }
$fixture = Join-Path $root ('build/test-data-' + [Guid]::NewGuid().ToString('N'))
& './build/keyline_tests.exe' $fixture
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
if ($TestOnly) { exit 0 }
$ray = Join-Path $root 'vendor/raylib-5.5_win64_mingw-w64'
if (-not (Test-Path -LiteralPath (Join-Path $ray 'include/raylib.h'))) {
    New-Item -ItemType Directory -Force -Path 'vendor' | Out-Null
    $zip = Join-Path $root 'vendor/raylib-5.5-win64.zip'
    if (-not (Test-Path -LiteralPath $zip)) {
        Write-Host 'Downloading official raylib 5.5 Windows C library (first build only)...'
        Invoke-WebRequest -Uri 'https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip' -OutFile $zip -TimeoutSec 180
    }
    $expected = 'FE8FEDA1B92FC02826E2D19C631D648FD0563B93B144893FE18140E9FE4FB6FD'
    if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $expected) { throw 'raylib download is incomplete or its checksum differs. Remove vendor/raylib-5.5-win64.zip and retry.' }
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $root 'vendor') -Force
}
$lib = Join-Path $ray 'lib/libraylib.a'
$inputs = @(Get-ChildItem -LiteralPath 'src' -File | Sort-Object Name | ForEach-Object { $_.FullName }) + @((Join-Path $root 'scripts/build.ps1'),(Join-Path $root 'run.bat'),(Join-Path $root 'CMakeLists.txt'))
$sourceLines = $inputs | ForEach-Object { (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash }
$hasher = [Security.Cryptography.SHA256]::Create()
$sourceHash = ([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes(($sourceLines -join "`n"))))).Replace('-','')
$hasher.Dispose()
$tag = $sourceHash.Substring(0,12)
$buildHeader = Join-Path $root 'build/keyline_build.h'
[IO.File]::WriteAllText($buildHeader, ('#define APP_BUILD_TAG "' + $tag + '"' + "`n"), (New-Object Text.UTF8Encoding($false)))
& gcc @warnings '-O2' '-Isrc' "-I$ray/include" '-include' $buildHeader @core 'src/main.c' 'src/app.c' 'src/ui.c' 'src/desktop.c' $lib '-o' 'build/keyline-new.exe' '-mwindows' '-static' '-static-libgcc' '-lopengl32' '-lgdi32' '-lwinmm' '-luser32' '-lm'
if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed.' }
try { Move-Item -LiteralPath 'build/keyline-new.exe' -Destination 'build/keyline.exe' -Force }
catch { throw 'Close the running development copy of Keyline, then build again. The previous executable was not launched.' }
$manifest = [ordered]@{ Version='1.0.2'; BuildTag=$tag; SourceSHA256=$sourceHash; ExecutableSHA256=(Get-FileHash -LiteralPath 'build/keyline.exe').Hash; BuiltAtUTC=[DateTime]::UtcNow.ToString('o'); Executable=(Join-Path $root 'build/keyline.exe') }
$manifest | ConvertTo-Json | Set-Content -LiteralPath 'build/keyline-build.json' -Encoding UTF8
Write-Host "Built build/keyline.exe (1.0.2, source $tag). Manifest: build/keyline-build.json"
if ($Run) {
    if ($DataDir) { & './build/keyline.exe' '--data-dir' $DataDir }
    else { & './build/keyline.exe' }
}
