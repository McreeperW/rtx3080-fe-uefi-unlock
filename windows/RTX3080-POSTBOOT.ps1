# Stage 14 read-only Windows-side verifier. Run from USB ROOT after Windows starts.
# No device state changes, firmware writes, or driver operations are performed.
$ErrorActionPreference = 'Stop'
$expectedEfiSha256 = 'e3f4de50a3991962158b645c977dbdf9a5a9db5afd964c3bf1b324efc9ffbbb4'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$results = New-Object System.Collections.ArrayList
function Add-Check([string]$name, [bool]$pass, [string]$detail) {
    $state = if ($pass) { 'PASS' } else { 'FAIL' }
    [void]$script:results.Add([pscustomobject]@{ State=$state; Name=$name; Detail=$detail })
    Write-Host ('[{0}] {1}: {2}' -f $state,$name,$detail)
}
function Has-ExactLine([string]$content, [string]$line) {
    return [bool]($content -match ('(?m)^' + [regex]::Escape($line) + '\r?$'))
}
Write-Host 'RTX 3080 FE Stage 14: Windows post-boot verifier'
Write-Host ('Source: {0}' -f $root)
$efi = Join-Path $root 'EFI\BOOT\BOOTX64.EFI'
if (Test-Path -LiteralPath $efi) {
    $hash = (Get-FileHash -LiteralPath $efi -Algorithm SHA256).Hash.ToLowerInvariant()
    Add-Check 'Stage14 EFI file hash' ($hash -eq $expectedEfiSha256) $hash
} else { Add-Check 'Stage14 EFI file hash' $false 'EFI/BOOT/BOOTX64.EFI not present at script root' }
$logs = @(Get-ChildItem -LiteralPath $root -Filter 'RTX3080-BOOT-*.TXT' -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending)
$foundLog = $null
foreach ($item in $logs) {
    $tryText = Get-Content -LiteralPath $item.FullName -Raw -ErrorAction SilentlyContinue
    if ($tryText -and $tryText.Contains('RTX3080 FE stage14: OWNERSHIP GUARD + STAGE13B WINDOWS HANDOFF')) {
        $foundLog = $item
        break
    }
}
if ($null -ne $foundLog) {
    $logText = Get-Content -LiteralPath $foundLog.FullName -Raw
    Add-Check 'Stage14 USB log exists' $true $foundLog.Name
    $maskMatch = [regex]::Match($logText, '(?m)^stage14_preflight_deny_mask=0x([0-9a-fA-F]{1,16})\r?$')
    $maskOk = $maskMatch.Success -and ([Convert]::ToUInt64($maskMatch.Groups[1].Value,16) -ne 0)
    Add-Check 'Unlock blocked by safety gate' $maskOk ($(if($maskMatch.Success){$maskMatch.Value.Trim()}else{'deny mask missing'}))
    $flagsOk = (Has-ExactLine $logText 'stage14_stage12_code_linked=0x0000000000000000') -and
               (Has-ExactLine $logText 'stage14_gpu_reset_attempted=0x0000000000000000') -and
               (Has-ExactLine $logText 'stage14_unlock_attempted=0x0000000000000000') -and
               (Has-ExactLine $logText 'stage14_decision=SKIP_UNLOCK_BOOT_WINDOWS')
    Add-Check 'No unlock or GPU reset performed' $flagsOk 'stage12=0; reset=0; unlock=0; SKIP_UNLOCK_BOOT_WINDOWS'
    $foundBoot = Has-ExactLine $logText 'windows_bootmgr_discovered=1'
    Add-Check 'Windows Boot Manager located' $foundBoot 'BootOrder scan'
    $loadOk = (Has-ExactLine $logText 'windows_loadimage_status=0x0000000000000000') -or
              (Has-ExactLine $logText 'windows_final_loadimage_status=0x0000000000000000')
    Add-Check 'Windows Boot Manager LoadImage' $loadOk 'direct load or matching-ESP fallback'
} else {
    Add-Check 'Stage14 USB log exists' $false 'No Stage14 log found on this drive'
    Add-Check 'Unlock blocked by safety gate' $false 'Not verifiable'
    Add-Check 'No unlock or GPU reset performed' $false 'Not verifiable'
    Add-Check 'Windows Boot Manager located' $false 'Not verifiable'
    Add-Check 'Windows Boot Manager LoadImage' $false 'Not verifiable'
}
try {
    $w = Get-CimInstance -ClassName Win32_OperatingSystem
    Add-Check 'Windows successfully started' ($null -ne $w -and $null -ne $w.LastBootUpTime) ([string]$w.LastBootUpTime)
} catch { Add-Check 'Windows successfully started' $false $_.Exception.Message }
try {
    $gpu = @(Get-CimInstance -ClassName Win32_VideoController |
        Where-Object { $_.PNPDeviceID -match 'VEN_10DE&DEV_2206' })
    Add-Check 'RTX 3080 detected by Windows' ($gpu.Count -eq 1) (($gpu | ForEach-Object { '{0} / {1} / Driver {2}' -f $_.Name,$_.PNPDeviceID,$_.DriverVersion }) -join '; ')
} catch { Add-Check 'RTX 3080 detected by Windows' $false $_.Exception.Message }
$smiPath = $null
$cmd = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
if($cmd){ $smiPath = $cmd.Source }
if(-not $smiPath){
    foreach($candidate in @("$env:WINDIR\System32\nvidia-smi.exe", "$env:ProgramFiles\NVIDIA Corporation\NVSMI\nvidia-smi.exe")) {
        if(Test-Path -LiteralPath $candidate){ $smiPath = $candidate; break }
    }
}
if($smiPath) {
    try {
        $smiOutput = & $smiPath --query-gpu=name,pci.bus_id,driver_version --format=csv,noheader 2>&1
        $code = $LASTEXITCODE
        Add-Check 'NVIDIA driver responding' ($code -eq 0 -and [bool]($smiOutput -match 'RTX 3080')) ($smiOutput -join '; ')
    } catch { Add-Check 'NVIDIA driver responding' $false $_.Exception.Message }
} else { Add-Check 'NVIDIA driver responding' $false 'nvidia-smi.exe not found' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$outFile = Join-Path $root "RTX3080-STAGE14-WINDOWS-CHECK-$stamp.TXT"
$allPass = @($results | Where-Object State -ne 'PASS').Count -eq 0
$outLines = @(
    'RTX 3080 FE Stage 14 Windows boot validation',
    ('Timestamp={0}' -f (Get-Date -Format 'o')),
    ('Result={0}' -f $(if($allPass){'PASS_BOOT_VALIDATION'}else{'NEEDS_REVIEW'})),
    ('Log={0}' -f $(if($foundLog){$foundLog.Name}else{'NOT_FOUND'})),
    ''
)
foreach($r in $results){ $outLines += '[{0}] {1}: {2}' -f $r.State,$r.Name,$r.Detail }
$outLines += @('', 'Note: PASS_BOOT_VALIDATION does NOT imply GPU unlock is implemented or safe.')
$outLines | Set-Content -LiteralPath $outFile -Encoding UTF8
Write-Host ('Result: {0}' -f $(if($allPass){'PASS_BOOT_VALIDATION'}else{'NEEDS_REVIEW'}))
Write-Host ('Report saved to: {0}' -f $outFile)
if(-not $allPass){ exit 1 }
