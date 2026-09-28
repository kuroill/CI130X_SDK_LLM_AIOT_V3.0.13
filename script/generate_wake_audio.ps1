param(
    [string[]]$VoiceIds = @('嘉岚3.0', '雷军'),
    [ValidateSet('positive', 'negative', 'all')]
    [string]$Class = 'all',
    [ValidateSet('zh-CN', 'en-US', 'es-ES', 'all')]
    [string]$Language = 'all',
    [string]$ManifestPath,
    [string]$OutputRoot,
    [string]$SampleRoot,
    [int]$Limit = 0,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$cloneScript = Join-Path $PSScriptRoot 'clone_voice.ps1'
$manifestPath = if ($ManifestPath) { $ManifestPath } else { Join-Path $PSScriptRoot 'wake_audio_cases.json' }
$outputRoot = if ($OutputRoot) { $OutputRoot } else { Join-Path $PSScriptRoot 'output' }
$cases = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json

if ($Class -ne 'all') {
    $cases = @($cases | Where-Object { $_.class -eq $Class })
}
if ($Language -ne 'all') {
    $cases = @($cases | Where-Object { $_.lang -eq $Language })
}
if ($Limit -gt 0) {
    $cases = @($cases | Select-Object -First $Limit)
}

$created = 0
$skipped = 0
$failed = 0
foreach ($case in $cases) {
    foreach ($voice in $VoiceIds) {
        $safeVoice = [regex]::Replace($voice, '[<>:"/\\|?*\x00-\x1f]', '_').Trim(' ', '.')
        $safeText = [regex]::Replace($case.text, '[<>:"/\\|?*\x00-\x1f]', '_').Trim(' ', '.')
        if ($safeText.Length -gt 80) {
            $safeText = $safeText.Substring(0, 80).Trim(' ', '.')
        }
        $folder = [System.IO.Path]::Combine($outputRoot, [string]$case.class, [string]$case.lang, [string]$safeVoice)
        $destination = Join-Path $folder ("{0}-{1}-{2}.wav" -f $case.id, $safeVoice, $safeText)
        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            Write-Host "SKIP $($case.id) $voice : $destination"
            $skipped++
            continue
        }
        if ($DryRun) {
            Write-Host "PLAN $($case.id) $voice : $destination"
            continue
        }
        [System.IO.Directory]::CreateDirectory($folder) | Out-Null
        Write-Host "GENERATE $($case.id) $voice : $($case.text)"
        $cloneArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $cloneScript,
            '-VoiceId', $voice, '-Text', $case.text, '-OutputPath', $destination)
        if ($SampleRoot) { $cloneArgs += @('-SampleRoot', $SampleRoot) }
        & powershell.exe @cloneArgs
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $destination -PathType Leaf)) {
            $created++
        }
        else {
            $failed++
            Write-Warning "FAILED $($case.id) $voice"
        }
    }
}

Write-Host "SUMMARY created=$created skipped=$skipped failed=$failed"
if ($failed -gt 0) { exit 1 }
