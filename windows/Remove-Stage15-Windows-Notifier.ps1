$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
Remove-ItemProperty -Path $runKey -Name RTX3080Stage15FailureOnly -ErrorAction SilentlyContinue
Write-Host 'Stage 15 notification disabled.'
