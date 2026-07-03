@echo off
setlocal

rem Usage:
rem   insert_pch.bat [root-folder] [dry|apply]
rem Examples:
rem   insert_pch.bat                -> dry run in current directory
rem   insert_pch.bat C:\Proj        -> dry run in C:\Proj
rem   insert_pch.bat C:\Proj apply  -> actually insert pch includes

if "%~1"=="" (
  set "ROOT=%CD%"
) else (
  set "ROOT=%~1"
)

rem Default to dry run. Use 'apply' or '0' to write changes.
set "DRY=1"
if /I "%~2"=="apply" set "DRY=0"
if /I "%~2"=="0" set "DRY=0"

echo Root: "%ROOT%"
if "%DRY%"=="1" (
  echo Mode: DRY RUN (no files will be modified)
) else (
  echo Mode: APPLY CHANGES
)

set "ps1=%TEMP%\insert_pch_%RANDOM%.ps1"

rem Write the PowerShell script (escape < and > as ^< ^> so cmd doesn't treat them as redirections)
> "%ps1%" echo param([string]$root,[string]$dry)
>> "%ps1%" echo if (-not (Test-Path -LiteralPath $root)) { Write-Host ("Root path not found: " + $root); exit 1 }
>> "%ps1%" echo if ([string]::IsNullOrEmpty($dry)) { $dry = '1' }
>> "%ps1%" echo $files = Get-ChildItem -LiteralPath $root -Recurse -Filter '*.cpp' -File -ErrorAction SilentlyContinue
>> "%ps1%" echo if (-not $files) { Write-Host 'No .cpp files found.'; exit 0 }
>> "%ps1%" echo foreach ($f in $files) {
>> "%ps1%" echo     try { $text = Get-Content -Raw -LiteralPath $f.FullName -ErrorAction Stop } catch { Write-Warning ('Failed to read: ' + $f.FullName); continue }
>> "%ps1%" echo     if ($text -notmatch '(?m)^\s*#\s*include\s+["<]pch\.h[">]') {
>> "%ps1%" echo         if ($dry -eq '1') { Write-Host ('Would add pch to: ' + $f.FullName) } else {
>> "%ps1%" echo             $inc = '#include "pch.h"'
>> "%ps1%" echo             $new = $inc + [Environment]::NewLine + $text
>> "%ps1%" echo             try { Set-Content -LiteralPath $f.FullName -Value $new -Encoding UTF8 -ErrorAction Stop; Write-Host ('Inserted pch in: ' + $f.FullName) } catch { Write-Warning ('Failed to write: ' + $f.FullName) }
>> "%ps1%" echo         }
>> "%ps1%" echo     } else { Write-Host ('Skipped (already has pch): ' + $f.FullName) }
>> "%ps1%" echo }

powershell -NoProfile -ExecutionPolicy Bypass -File "%ps1%" -root "%ROOT%" -dry 0

del "%ps1%" >nul 2>&1
endlocal