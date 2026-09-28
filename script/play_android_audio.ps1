param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$AudioPath,

    [string]$Serial,
    [string]$AdbPath = 'adb'
)

$ErrorActionPreference = 'Stop'

function Invoke-Adb {
    param([string[]]$Arguments)

    # Windows PowerShell treats adb's progress text on stderr as a terminating
    # error when ErrorActionPreference is Stop, even if adb exits successfully.
    $ErrorActionPreference = 'Continue'
    $commandArgs = @()
    if ($Serial) {
        $commandArgs += @('-s', $Serial)
    }
    $commandArgs += $Arguments

    $output = & $AdbPath @commandArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ADB command failed (exit $LASTEXITCODE): $($output -join [Environment]::NewLine)"
    }
    return $output
}

try {
    $resolvedPath = (Resolve-Path -LiteralPath $AudioPath).ProviderPath
    $item = Get-Item -LiteralPath $resolvedPath

    $mimeTypes = @{
        '.mp3' = 'audio/mpeg'
        '.wav' = 'audio/wav'
        '.m4a' = 'audio/mp4'
        '.aac' = 'audio/aac'
        '.ogg' = 'audio/ogg'
        '.flac' = 'audio/flac'
    }

    if ($item.PSIsContainer) {
        $files = @(Get-ChildItem -LiteralPath $resolvedPath -Recurse -File |
            Where-Object {
                -not $_.Name.StartsWith('._') -and
                $mimeTypes.ContainsKey($_.Extension.ToLowerInvariant())
            } |
            Sort-Object FullName)
        if ($files.Count -eq 0) {
            throw "No supported audio files found in: $resolvedPath"
        }

        for ($i = 0; $i -lt $files.Count; $i++) {
            $relativeName = $files[$i].FullName.Substring($resolvedPath.Length).TrimStart('\')
            Write-Host ('{0,3}. {1}' -f ($i + 1), $relativeName)
        }
        $choice = Read-Host 'Enter the audio number to play'
        $number = 0
        if (-not [int]::TryParse($choice, [ref]$number) -or
            $number -lt 1 -or $number -gt $files.Count) {
            throw "Invalid audio number: $choice"
        }
        $item = $files[$number - 1]
    }

    $extension = $item.Extension.ToLowerInvariant()
    if (-not $mimeTypes.ContainsKey($extension)) {
        throw "Unsupported audio format: $extension"
    }

    if (-not (Get-Command -Name $AdbPath -CommandType Application -ErrorAction SilentlyContinue)) {
        throw "adb was not found: $AdbPath. Install Android platform-tools or pass -AdbPath."
    }

    $devices = @(& $AdbPath devices)
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to list ADB devices.'
    }
    $online = @($devices | Where-Object { $_ -match '^[^\s]+\s+device\s*$' } |
        ForEach-Object { ($_ -split '\s+')[0] })
    if ($Serial) {
        if ($online -notcontains $Serial) {
            throw "ADB device is not online: $Serial"
        }
    }
    elseif ($online.Count -ne 1) {
        throw "Expected one online ADB device; found $($online.Count). Use -Serial when multiple devices are connected."
    }

    $remoteDir = '/sdcard/Download/CodexAudioTest'
    $remoteFile = "$remoteDir/wake_test_$([DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff'))$extension"
    $uri = "file://$remoteFile"

    Write-Host "Audio: $($item.FullName)"
    Write-Host "Device: $(if ($Serial) { $Serial } else { $online[0] })"
    [void](Invoke-Adb -Arguments @('shell', 'mkdir', '-p', $remoteDir))
    [void](Invoke-Adb -Arguments @('push', $item.FullName, $remoteFile))

    $result = Invoke-Adb -Arguments @(
        'shell', 'am', 'start', '-W',
        '-a', 'android.intent.action.VIEW',
        '-d', $uri,
        '-t', $mimeTypes[$extension],
        '--ez', 'android.intent.extra.START_PLAYBACK', 'true'
    )
    $result | ForEach-Object { Write-Host $_ }
    if (@($result | Where-Object {
        $_ -match '^(Error:|.*unable to resolve Intent|.*No Activity found)'
    }).Count -gt 0) {
        throw 'Android could not open the audio file. Install or select an audio player that handles ACTION_VIEW.'
    }

    Start-Sleep -Milliseconds 300
    try {
        [void](Invoke-Adb -Arguments @('shell', 'input', 'keyevent', '126'))
    }
    catch {
        Write-Warning 'The Android media Play key was not accepted; use the player Play button.'
    }
    Write-Host "Opened on Android: $uri"
    Write-Host 'If the player opens without playing, tap Play on the device. Autoplay depends on the installed player.'
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
