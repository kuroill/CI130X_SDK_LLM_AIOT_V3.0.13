param(
    [string]$Port = 'COM3',
    [int]$Baud = 921600,
    [string[]]$VoiceIds = @('嘉岚3.0', '雷军'),
    [ValidateSet('positive', 'negative', 'all')]
    [string]$Class = 'all',
    [ValidateSet('zh-CN', 'en-US', 'es-ES', 'all')]
    [string]$Language = 'all',
    [string]$ManifestPath,
    [string]$AudioRoot,
    [string]$DirectAudioPath,
    [string]$DirectText = 'Vitana',
    [int]$Repeats = 2,
    [int]$Limit = 0,
    [int]$NoResultWaitMs = 8000,
    [int]$WakeRecoveryMs = 8000,
    [string]$ResultDir
)

$ErrorActionPreference = 'Stop'
if ($Repeats -lt 1 -or $NoResultWaitMs -lt 3000 -or $WakeRecoveryMs -lt 3000) {
    throw 'Repeats must be positive; wait times must be at least 3000 ms.'
}

$directAudio = if ($DirectAudioPath) { (Resolve-Path -LiteralPath $DirectAudioPath).ProviderPath } else { $null }
if ($directAudio -and [IO.Path]::GetExtension($directAudio).ToLowerInvariant() -notin @('.wav', '.m4a', '.mp3')) {
    throw 'DirectAudioPath must be WAV, M4A, or MP3.'
}
$manifestPath = if ($ManifestPath) { $ManifestPath } else { Join-Path $PSScriptRoot 'wake_audio_cases.json' }
$audioRoot = if ($AudioRoot) { $AudioRoot } else { Join-Path $PSScriptRoot 'output' }
if ($directAudio) {
    $manifest = @([pscustomobject]@{ id = 'P-DIRECT'; class = 'positive'; lang = 'es-ES'; text = $DirectText })
    $VoiceIds = @('direct')
} else {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Class -ne 'all') { $manifest = @($manifest | Where-Object { $_.class -eq $Class }) }
    if ($Language -ne 'all') { $manifest = @($manifest | Where-Object { $_.lang -eq $Language }) }
    if ($Limit -gt 0) { $manifest = @($manifest | Select-Object -First $Limit) }
}

$trials = New-Object System.Collections.Generic.List[object]
foreach ($item in $manifest) {
    foreach ($voice in $VoiceIds) {
        if ($directAudio) {
            $file = $directAudio
        } else {
            $folder = [IO.Path]::Combine($audioRoot, [string]$item.class, [string]$item.lang, [string]$voice)
            $files = @(Get-ChildItem -LiteralPath $folder -File -Filter ("{0}-{1}-*.wav" -f $item.id, $voice) -ErrorAction SilentlyContinue)
            if ($files.Count -ne 1) { throw "Expected one WAV for $($item.id) / $voice; found $($files.Count)." }
            $file = $files[0].FullName
        }
        for ($repeat = 1; $repeat -le $Repeats; $repeat++) {
            $trials.Add([pscustomobject]@{
                Case = $item; Voice = $voice; Repeat = $repeat; Path = $file
            })
        }
    }
}

if (-not $ResultDir) {
    $ResultDir = Join-Path $PSScriptRoot ('output\test_runs\' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
}
$ResultDir = [IO.Path]::GetFullPath($ResultDir)
[IO.Directory]::CreateDirectory($ResultDir) | Out-Null
$csvPath = Join-Path $ResultDir 'results.csv'
$logPath = Join-Path $ResultDir 'serial.log'
if ((Test-Path -LiteralPath $csvPath) -or (Test-Path -LiteralPath $logPath)) {
    throw "Result files already exist in: $ResultDir"
}

$script:lineBuffer = New-Object System.Text.StringBuilder
$log = New-Object System.IO.StreamWriter($logPath, $false, (New-Object System.Text.UTF8Encoding($true)))
$log.AutoFlush = $true
$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, ([IO.Ports.Parity]::None), 8, ([IO.Ports.StopBits]::One)
$sp.ReadTimeout = 100
$sp.ReadBufferSize = 16384

function Read-SerialLines {
    param([IO.Ports.SerialPort]$Serial, [IO.StreamWriter]$Writer)
    $lines = New-Object System.Collections.Generic.List[string]
    $available = $Serial.BytesToRead
    if ($available -le 0) { return $lines }
    $data = New-Object byte[] $available
    $read = $Serial.Read($data, 0, $data.Length)
    for ($i = 0; $i -lt $read; $i++) {
        $byte = [int]$data[$i]
        if ($byte -eq 10 -or $byte -eq 13) {
            if ($script:lineBuffer.Length -gt 0) {
                $line = $script:lineBuffer.ToString()
                [void]$script:lineBuffer.Clear()
                $Writer.WriteLine(('{0:o} {1}' -f (Get-Date), $line))
                $lines.Add($line)
            }
        }
        elseif ($byte -ge 32 -and $byte -le 126) {
            [void]$script:lineBuffer.Append([char]$byte)
            if ($script:lineBuffer.Length -gt 1000) { [void]$script:lineBuffer.Clear() }
        }
    }
    return $lines
}

function Drain-For {
    param([IO.Ports.SerialPort]$Serial, [IO.StreamWriter]$Writer, [int]$Milliseconds)
    $until = (Get-Date).AddMilliseconds($Milliseconds)
    while ((Get-Date) -lt $until) {
        [void](Read-SerialLines -Serial $Serial -Writer $Writer)
        Start-Sleep -Milliseconds 50
    }
}

try {
    $sp.Open()
    Write-Host "Port=$Port Baud=$Baud Trials=$($trials.Count) Results=$ResultDir"
    $log.WriteLine(('{0:o} TEST_START port={1} baud={2} trials={3}' -f (Get-Date), $Port, $Baud, $trials.Count))
    Drain-For -Serial $sp -Writer $log -Milliseconds 2000
    $trialNo = 0
    foreach ($trial in $trials) {
        $trialNo++
        $item = $trial.Case
        $label = '{0}/{1} {2} {3} repeat={4}' -f $trialNo, $trials.Count, $item.id, $trial.Voice, $trial.Repeat
        Drain-For -Serial $sp -Writer $log -Milliseconds 1000
        $sp.DiscardInBuffer()
        [void]$script:lineBuffer.Clear()
        $log.WriteLine(('{0:o} TRIAL_START {1} text={2} file={3}' -f (Get-Date), $label, $item.text, $trial.Path))
        Write-Host "PLAY $label : $($item.text)"

        $duringPlayback = New-Object System.Collections.Generic.List[string]
        if ([IO.Path]::GetExtension($trial.Path).ToLowerInvariant() -eq '.wav') {
            $player = New-Object System.Media.SoundPlayer $trial.Path
            $playedAt = Get-Date
            $player.Load()
            $player.PlaySync()
            $playEndedAt = Get-Date
        } else {
            Add-Type -AssemblyName PresentationCore
            $player = New-Object System.Windows.Media.MediaPlayer
            try {
                $player.Open([Uri]$trial.Path)
                $openDeadline = (Get-Date).AddSeconds(5)
                while (-not $player.HasAudio -or -not $player.NaturalDuration.HasTimeSpan) {
                    if ((Get-Date) -ge $openDeadline) { throw "Media failed to open: $($trial.Path)" }
                    Start-Sleep -Milliseconds 50
                }
                $durationMs = [int]$player.NaturalDuration.TimeSpan.TotalMilliseconds
                $player.Volume = 1.0
                $playedAt = Get-Date
                $player.Play()
                $playUntil = $playedAt.AddMilliseconds($durationMs + 150)
                while ((Get-Date) -lt $playUntil) {
                    foreach ($line in (Read-SerialLines -Serial $sp -Writer $log)) { $duringPlayback.Add($line) }
                    Start-Sleep -Milliseconds 40
                }
                $playEndedAt = Get-Date
            }
            finally {
                $player.Stop()
                $player.Close()
            }
        }
        $observed = ''
        $resultAt = $null
        $aecIdleAt = $null
        $deadline = $playEndedAt.AddMilliseconds($NoResultWaitMs)
        foreach ($line in $duringPlayback) {
            if (-not $observed -and $line -match 'send result:\s*([A-Za-z0-9_-]+)') {
                $observed = $Matches[1]
                $resultAt = $playEndedAt
                $deadline = $resultAt.AddMilliseconds($WakeRecoveryMs)
            }
            if ($line -match '\[AEC\] work_state=0 play_state=0') { $aecIdleAt = $playEndedAt }
        }
        while ((Get-Date) -lt $deadline) {
            $lines = Read-SerialLines -Serial $sp -Writer $log
            foreach ($line in $lines) {
                if (-not $observed -and $line -match 'send result:\s*([A-Za-z0-9_-]+)') {
                    $observed = $Matches[1]
                    $resultAt = Get-Date
                    $deadline = $resultAt.AddMilliseconds($WakeRecoveryMs)
                }
                if ($line -match '\[AEC\] work_state=0 play_state=0') {
                    $aecIdleAt = Get-Date
                }
            }
            if ($observed -and $aecIdleAt -and
                (Get-Date) -ge $resultAt.AddMilliseconds(3000) -and
                (Get-Date) -ge $aecIdleAt.AddMilliseconds(2000)) { break }
            Start-Sleep -Milliseconds 50
        }
        [void](Read-SerialLines -Serial $sp -Writer $log)

        $pass = if ($item.class -eq 'positive') { $observed -eq 'VITANA' } else { -not $observed }
        $latencyMs = if ($resultAt) { [int](($resultAt - $playedAt).TotalMilliseconds) } else { '' }
        $row = [pscustomobject]@{
            Timestamp = $playedAt.ToString('o')
            CaseId = $item.id
            Class = $item.class
            Language = $item.lang
            VoiceId = $trial.Voice
            Text = $item.text
            Repeat = $trial.Repeat
            File = $trial.Path
            PlayDurationMs = [int](($playEndedAt - $playedAt).TotalMilliseconds)
            Observed = if ($observed) { $observed } else { 'NO_RESULT' }
            LatencyMs = $latencyMs
            Passed = $pass
        }
        if (Test-Path -LiteralPath $csvPath) {
            $row | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8 -Append
        } else {
            $row | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
        }
        $log.WriteLine(('{0:o} TRIAL_END {1} observed={2} passed={3}' -f (Get-Date), $label, $row.Observed, $pass))
        Write-Host "RESULT $label observed=$($row.Observed) passed=$pass"
    }
    Write-Host "DONE results=$csvPath serial=$logPath"
}
finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
    $log.Dispose()
}
