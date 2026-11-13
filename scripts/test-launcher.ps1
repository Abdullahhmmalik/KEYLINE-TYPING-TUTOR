$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $root ('build/launcher-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'build') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'run.bat') -Destination (Join-Path $fixture 'run.bat')
$program = '#include <stdio.h>
int main(void) { FILE *f=fopen("launch-proof.txt","w"); if(!f) return 1; fputs("VALUE",f); return fclose(f)!=0; }
'
$source = Join-Path $fixture 'launcher.c'
$executable = Join-Path $fixture 'build/keyline.exe'
[IO.File]::WriteAllText($source,$program.Replace('VALUE','OLD'))
& gcc '-std=c99' '-Wall' '-Wextra' '-Wpedantic' '-mwindows' $source '-o' $executable
if ($LASTEXITCODE -ne 0) { throw 'Launcher fixture compilation failed.' }
[IO.File]::WriteAllText($source,$program.Replace('VALUE','NEW'))
$buildStub = Join-Path $fixture 'build.bat'
[IO.File]::WriteAllText($buildStub,"@echo off`r`ngcc -std=c99 -mwindows launcher.c -o build\keyline.exe`r`nexit /b %errorlevel%`r`n")
$command = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c','run.bat') -WorkingDirectory $fixture -WindowStyle Hidden -Wait -PassThru
if ($command.ExitCode -ne 0) { throw 'Development launcher failed in the isolated fixture.' }
$proof = Join-Path $fixture 'launch-proof.txt'
for ($i=0;$i -lt 40 -and -not (Test-Path -LiteralPath $proof);$i++) { Start-Sleep -Milliseconds 100 }
if (-not (Test-Path -LiteralPath $proof) -or [IO.File]::ReadAllText($proof) -ne 'NEW') { throw 'Stale executable launched: expected the updated source.' }
[IO.File]::WriteAllText($proof,'UNCHANGED')
[IO.File]::WriteAllText($buildStub,"@echo off`r`nexit /b 7`r`n")
$command = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c','run.bat') -WorkingDirectory $fixture -WindowStyle Hidden -Wait -PassThru
Start-Sleep -Milliseconds 300
if ($command.ExitCode -eq 0 -or [IO.File]::ReadAllText($proof) -ne 'UNCHANGED') { throw 'A failed build incorrectly launched the existing executable.' }
Write-Host 'Launcher checks: 2 passed (updated source replaces existing EXE; failed build never falls back).'
Write-Host "Isolated fixtures: $fixture"
