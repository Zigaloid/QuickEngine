param(
  [string]$Root = ".",
  [switch]$Apply
)

if (-not (Test-Path -LiteralPath $Root)) {
  Write-Error "Root path not found: $Root"
  exit 1
}

$files = Get-ChildItem -LiteralPath $Root -Recurse -Filter '*.cpp' -File -ErrorAction SilentlyContinue
if (-not $files) {
  Write-Host "No .cpp files found under $Root"
  exit 0
}

foreach ($f in $files) {
  try {
    $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
  } catch {
    Write-Warning "Failed to read: $($f.FullName)"
    continue
  }

  # Detect encoding by BOM
  $enc = [System.Text.Encoding]::Default
  if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $enc = [System.Text.Encoding]::UTF8 }
  elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) { $enc = [System.Text.Encoding]::Unicode }
  elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) { $enc = [System.Text.Encoding]::BigEndianUnicode }
  elseif ($bytes.Length -ge 4 -and $bytes[0] -eq 0x00 -and $bytes[1] -eq 0x00 -and $bytes[2] -eq 0xFE -and $bytes[3] -eq 0xFF) { $enc = [System.Text.Encoding]::GetEncoding("utf-32") }

  $text = $enc.GetString($bytes)

  if ($text -match '(?m)^\s*#\s*include\s+[\"<]pch\.h[\">]') {
    Write-Host "Skipped (already has pch): $($f.FullName)"
    continue
  }

  if (-not $Apply) {
    Write-Host "Would add pch to: $($f.FullName)"
    continue
  }

  $inc = '#include "pch.h"' + [Environment]::NewLine
  $newText = $inc + $text
  $outBytes = $enc.GetPreamble() + $enc.GetBytes($newText)

  try {
    [System.IO.File]::WriteAllBytes($f.FullName, $outBytes)
    Write-Host "Inserted pch in: $($f.FullName)"
  } catch {
    Write-Warning "Failed to write: $($f.FullName)"
  }
}