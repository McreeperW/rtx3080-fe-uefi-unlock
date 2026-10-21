# Install a current-user Windows logon watcher; no administrator rights required.
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot 'RTX3080-STAGE15-NOTIFY.ps1'
if (-not (Test-Path -LiteralPath $src)) { throw 'Notification script missing in this folder.' }
$dir = Join-Path $env:LOCALAPPDATA 'RTX3080Stage15'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$dst = Join-Path $dir 'Notify.ps1'
Copy-Item -LiteralPath $src -Destination $dst -Force
$cmd = ('powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}"' -f $dst)
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
New-ItemProperty -Path $runKey -Name RTX3080Stage15FailureOnly -PropertyType String -Value $cmd -Force | Out-Null
Write-Host 'Stage 15 failure-only Windows notification installed for current user.'
Write-Host 'On Windows logon: if a new Stage15 error exists on a removable USB, Notepad opens the failure log.'
