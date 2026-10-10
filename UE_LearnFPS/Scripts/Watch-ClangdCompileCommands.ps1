#Requires -Version 5.1
<#
.SYNOPSIS
  Regenerate compile_commands.json when C++ sources or UE rsp files change.
#>
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Refresh = Join-Path $PSScriptRoot "Refresh-ClangdCompileCommands.ps1"

Write-Host "clangd-watch: watching $ProjectRoot"
Write-Host "clangd-watch: idle"

function Invoke-Refresh {
  try {
    & $Refresh -ProjectRoot $ProjectRoot
  } catch {
    Write-Host "clangd-watch: $($_.Exception.Message)"
  }
}

Invoke-Refresh

$watchDirs = @(
  (Join-Path $ProjectRoot "Source"),
  (Join-Path $ProjectRoot ".vscode")
) | Where-Object { Test-Path $_ }

$watchers = foreach ($dir in $watchDirs) {
  $watcher = New-Object System.IO.FileSystemWatcher
  $watcher.Path = $dir
  $watcher.IncludeSubdirectories = $true
  $watcher.NotifyFilter = [IO.NotifyFilters]'FileName, LastWrite, Size, CreationTime'
  $watcher.EnableRaisingEvents = $true
  $id = "clangd-" + [guid]::NewGuid().ToString("N")
  foreach ($eventName in @("Created", "Changed", "Deleted", "Renamed")) {
    Register-ObjectEvent -InputObject $watcher -EventName $eventName -SourceIdentifier "$id-$eventName" | Out-Null
  }
  $watcher
}

$pendingSince = [datetime]::MinValue
try {
  while ($true) {
    $event = Wait-Event -Timeout 1
    if ($event) {
      $path = $event.SourceEventArgs.FullPath
      if (-not $path) { $path = $event.SourceEventArgs.Name }
      Remove-Event -EventIdentifier $event.EventIdentifier
      if ($path -match '\.(h|hpp|c|cc|cpp|cxx|rsp)$') {
        $pendingSince = Get-Date
      }
    } elseif ($pendingSince -ne [datetime]::MinValue -and ((Get-Date) - $pendingSince).TotalSeconds -ge 1.5) {
      $pendingSince = [datetime]::MinValue
      Invoke-Refresh
    }
  }
} finally {
  Get-EventSubscriber | Where-Object { $_.SourceIdentifier -like "clangd-*" } | Unregister-Event -ErrorAction SilentlyContinue
  foreach ($watcher in $watchers) { $watcher.Dispose() }
}
