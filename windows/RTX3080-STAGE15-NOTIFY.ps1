# Stage15 Windows logon notifier. Shows ONLY a failed or interrupted Stage15 attempt.
$ErrorActionPreference = 'SilentlyContinue'
$stateDir = Join-Path $env:LOCALAPPDATA 'RTX3080Stage15'
New-Item -ItemType Directory -Force -Path $stateDir | Out-Null
$seenFile = Join-Path $stateDir 'seen.txt'
$seen = @{}
if (Test-Path -LiteralPath $seenFile) {
  Get-Content -LiteralPath $seenFile | ForEach-Object { if($_){$seen[$_] = $true} }
}
$logs = @()
foreach ($drive in [System.IO.DriveInfo]::GetDrives()) {
  if (-not $drive.IsReady -or $drive.DriveType -notin @('Removable','Fixed')) { continue }
  $logs += @(Get-ChildItem -LiteralPath $drive.RootDirectory.FullName -Filter 'RTX3080-STAGE15-*.TXT' -File -ErrorAction SilentlyContinue)
}
$failures = @()
foreach ($file in @($logs | Sort-Object LastWriteTimeUtc)) {
  $key = '{0}|{1}|{2}' -f $file.FullName,$file.Length,$file.LastWriteTimeUtc.Ticks
  if ($seen.ContainsKey($key)) { continue }
  $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
  if (-not $content -or -not $content.Contains('RTX3080 FE STAGE15 HEADLESS USB BRIDGE')) { continue }
  $results = @([regex]::Matches($content, '(?m)^stage15_result=([^\r\n]+)') | ForEach-Object { $_.Groups[1].Value })
  $last = if($results.Count -gt 0){$results[-1]}else{'UNKNOWN'}
  $isFail = $last.StartsWith('FAILED_') -or $last -eq 'ATTEMPT_STARTED_NOT_YET_RETURNED'
  $seen[$key] = $true
  if ($isFail) { $failures += [pscustomobject]@{File=$file;Result=$last;Content=$content} }
}
$seen.Keys | Sort-Object | Set-Content -LiteralPath $seenFile -Encoding UTF8
if ($failures.Count -gt 0) {
  $latest = @($failures)[-1]
  $summary = Join-Path $stateDir 'LAST-STAGE15-FAILURE.txt'
  @(
    'RTX 3080 FE Stage 15 failed or did not return.',
    ('Result={0}' -f $latest.Result),
    ('Source={0}' -f $latest.File.FullName),
    'The one-shot arm marker was consumed before invocation, to avoid repeated attempts.',
    'A GPU/firmware hang cannot be repaired by the boot bridge.',
    '',
    $latest.Content
  ) | Set-Content -LiteralPath $summary -Encoding UTF8
  Start-Process -FilePath notepad.exe -ArgumentList ('"{0}"' -f $summary)
}
