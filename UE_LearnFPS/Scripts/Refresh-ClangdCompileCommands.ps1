#Requires -Version 5.1
<#
.SYNOPSIS
  Rebuild compile_commands.json so clangd works with Unreal Engine C++ in Cursor.

.DESCRIPTION
  UE generates MSVC @rsp IntelliSense data that clangd cannot use well.
  This script expands the rsp, drops SharedPCH, forces C++20, and writes
  compile_commands.json at the project root.

.PARAMETER ProjectRoot
  Folder that contains the .uproject file. Default: parent of Scripts/, or current directory.
#>
[CmdletBinding()]
param(
  [string]$ProjectRoot
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
  param([string]$Hint)
  if ($Hint -and (Test-Path $Hint)) {
    $p = (Resolve-Path $Hint).Path
    if (Get-ChildItem $p -Filter *.uproject -File -ErrorAction SilentlyContinue) { return $p }
  }
  if ($PSScriptRoot) {
    $candidate = Split-Path $PSScriptRoot -Parent
    if (Get-ChildItem $candidate -Filter *.uproject -File -ErrorAction SilentlyContinue) { return $candidate }
  }
  $cwd = (Get-Location).Path
  if (Get-ChildItem $cwd -Filter *.uproject -File -ErrorAction SilentlyContinue) { return $cwd }
  throw "Cannot find a .uproject. Pass -ProjectRoot 'D:\YourGame'."
}

function Find-ClExe {
  $preferred = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
    "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe"
  )
  foreach ($pat in $preferred) {
    $hit = Get-Item $pat -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
    if ($hit) { return $hit.FullName }
  }
  $found = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter cl.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match 'Hostx64\\x64\\cl\.exe' } |
    Select-Object -First 1 -ExpandProperty FullName
  if (-not $found) { throw "cl.exe not found. Install Visual Studio C++ workload." }
  return $found
}

function Find-RspInDirectory {
  param([string]$Dir)
  if (-not (Test-Path $Dir)) { return $null }
  foreach ($filter in @("*.1.rsp", "*.rsp")) {
    $rsp = Get-ChildItem $Dir -Filter $filter -File -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($rsp) { return $rsp.FullName }
  }
  return $null
}

function Find-RspFile {
  param([string]$Root)
  $vscode = Join-Path $Root ".vscode"
  if (-not (Test-Path $vscode)) {
    throw "Missing .vscode — in Unreal Editor: Tools > Refresh Visual Studio Code Project"
  }

  # Prefer module-named compileCommands_<GameModule> over Default.
  # UE 5.8 writes *.0.rsp; older projects write *.1.rsp.
  $dirs = Get-ChildItem $vscode -Directory -Filter "compileCommands_*" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne "compileCommands_Default" } |
    Sort-Object Name

  foreach ($d in $dirs) {
    $rsp = Find-RspInDirectory -Dir $d.FullName
    if ($rsp) { return $rsp }
  }

  $defaultRsp = Find-RspInDirectory -Dir (Join-Path $vscode "compileCommands_Default")
  if ($defaultRsp) { return $defaultRsp }

  throw "No .rsp under .vscode\compileCommands_* — Refresh Visual Studio Code Project in UE first."
}

function Get-ProjectSources {
  param([string]$Root)
  $sourceRoot = Join-Path $Root "Source"
  if (-not (Test-Path $sourceRoot)) { throw "Missing Source\ folder." }
  $files = @(Get-ChildItem $sourceRoot -Recurse -Include *.cpp,*.h,*.hpp,*.c,*.cc,*.cxx -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\Intermediate\\' })
  if ($files.Count -eq 0) { throw "No C/C++ sources under Source\." }
  return $files.FullName
}

function Convert-RspToArgs {
  param([string]$RspPath)
  $flagArgs = New-Object System.Collections.Generic.List[string]
  Get-Content $RspPath | ForEach-Object {
    $line = $_.Trim()
    if (-not $line) { return }
    if ($line -match 'SharedPCH') { return }
    if ($line -match '^/Y[cu]' -or $line -match '^/Fp' -or $line -match '^/Fo' -or $line -match '^/Fd') { return }

    if ($line -match '^(/(?:I|FI|imsvc|external:I))\s+"(.*)"\s*$') {
      $flagArgs.Add($Matches[1]); $flagArgs.Add($Matches[2])
    } elseif ($line -match '^(/\S+)\s+"(.*)"\s*$') {
      $flagArgs.Add($Matches[1]); $flagArgs.Add($Matches[2])
    } elseif ($line -match '^(/\S+)\s+(\S+)\s*$') {
      $flagArgs.Add($Matches[1]); $flagArgs.Add($Matches[2])
    } else {
      $flagArgs.Add($line.Trim('"'))
    }
  }
  return $flagArgs
}

# ---- main ----
$projectRoot = Resolve-ProjectRoot -Hint $ProjectRoot
$projectRoot = $projectRoot.TrimEnd('\', '/')
$rspPath = Find-RspFile -Root $projectRoot
$cl = Find-ClExe
$flagArgs = Convert-RspToArgs -RspPath $rspPath
$sources = @(Get-ProjectSources -Root $projectRoot)
$prefix = @('/std:c++20', '/Zc:preprocessor', '/EHsc', '/TP', '/DUNICODE', '/D_UNICODE')
$dirJson = ($projectRoot -replace '\\', '/')

Write-Host "Project : $projectRoot"
Write-Host "RSP     : $rspPath"
Write-Host "cl.exe  : $cl"
Write-Host "Sources : $($sources.Count)"

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine('[')
for ($i = 0; $i -lt $sources.Count; $i++) {
  [void]$sb.AppendLine('  {')
  [void]$sb.AppendLine(('    "directory": "' + $dirJson + '",'))
  [void]$sb.AppendLine(('    "file": "' + ($sources[$i] -replace '\\', '\\\\') + '",'))
  [void]$sb.Append('    "arguments": [')
  $all = @($cl) + $prefix + @($flagArgs) + @('/c', $sources[$i])
  for ($j = 0; $j -lt $all.Count; $j++) {
    if ($j -gt 0) { [void]$sb.Append(', ') }
    [void]$sb.Append('"' + ($all[$j] -replace '\\', '\\\\' -replace '"', '\"') + '"')
  }
  [void]$sb.AppendLine(']')
  if ($i -lt $sources.Count - 1) { [void]$sb.AppendLine('  },') } else { [void]$sb.AppendLine('  }') }
}
[void]$sb.AppendLine(']')

$out = Join-Path $projectRoot "compile_commands.json"
$newText = $sb.ToString()
if ((Test-Path $out) -and ([System.IO.File]::ReadAllText($out) -eq $newText)) {
  Write-Host "Unchanged -> $out"
  return
}
[System.IO.File]::WriteAllText($out, $newText, [System.Text.UTF8Encoding]::new($false))
Write-Host "OK -> $out"
