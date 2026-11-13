param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
$ray = Join-Path $root 'vendor/raylib-5.5_win64_mingw-w64'
if (-not (Test-Path -LiteralPath "$ray/lib/libraylib.a")) { throw 'Run build.bat first to obtain raylib.' }
$core = @('src/config.c','src/stats.c','src/typing.c','src/data.c','src/achievements.c')
& gcc '-std=c99' '-Wall' '-Wextra' '-Wpedantic' '-Wshadow' '-Wstrict-prototypes' '-O2' '-Isrc' "-I$ray/include" @core 'src/app.c' 'src/ui.c' 'src/desktop.c' 'tests/test_gui.c' 'tests/native_test_input.c' "$ray/lib/libraylib.a" '-o' 'build/keyline_gui_tests.exe' '-static' '-static-libgcc' '-lopengl32' '-lgdi32' '-lwinmm' '-luser32' '-lm'
if ($LASTEXITCODE -ne 0) { throw 'GUI test compilation failed.' }
$images = Join-Path $root 'build/gui-report'
$data = Join-Path $root ('build/gui-test-data-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $images | Out-Null
& './build/keyline_gui_tests.exe' $data $images
if ($LASTEXITCODE -ne 0) { throw 'GUI verification failed. Inspect build/gui-report and the reported checks.' }
Write-Host 'GUI verification passed. Screenshots: build/gui-report'
