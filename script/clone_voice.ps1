param(
    [Parameter(Mandatory = $true)]
    [string]$Text,

    [Parameter(Mandatory = $true)]
    [string]$VoiceId,

    [string]$SampleRoot = 'E:\Project\测试声音',
    [string]$EnvFile = 'E:\Project\GitProject\iot-service\.env',
    [string]$OutputPath,
    [string]$ApiUrl
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Net.Http

function Get-EnvValue {
    param([string]$Key, [string]$File)

    $value = [Environment]::GetEnvironmentVariable($Key)
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        return $value.Trim()
    }
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
        return ''
    }
    foreach ($line in [System.IO.File]::ReadAllLines($File)) {
        if ($line -match ('^\s*(?:export\s+)?' + [regex]::Escape($Key) + '\s*=\s*(.*?)\s*$')) {
            $value = $Matches[1]
            if ($value.Length -ge 2 -and
                (($value.StartsWith('"') -and $value.EndsWith('"')) -or
                 ($value.StartsWith("'") -and $value.EndsWith("'")))) {
                $value = $value.Substring(1, $value.Length - 2)
            }
            return $value.Trim()
        }
    }
    return ''
}

function Get-ResponseError {
    param([System.Net.Http.HttpResponseMessage]$Response)

    $body = $Response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
    if ($body.Length -gt 1000) {
        $body = $body.Substring(0, 1000)
    }
    return "HTTP $([int]$Response.StatusCode): $body"
}

function Send-VoiceRequest {
    param(
        [System.Net.Http.HttpClient]$Client,
        [string]$Url,
        [System.Net.Http.HttpContent]$Content
    )

    $request = [System.Net.Http.HttpRequestMessage]::new(
        [System.Net.Http.HttpMethod]::Post, $Url)
    $request.Headers.Add('x-request-id', [guid]::NewGuid().ToString())
    $request.Headers.Add('x-voice-priority', 'background')
    $request.Content = $Content
    try {
        return $Client.SendAsync($request).GetAwaiter().GetResult()
    }
    finally {
        $request.Dispose()
    }
}

try {
    if ([string]::IsNullOrWhiteSpace($Text)) {
        throw 'Text cannot be empty.'
    }
    if ([string]::IsNullOrWhiteSpace($VoiceId) -or
        $VoiceId -match '[\\/]' -or $VoiceId -eq '.' -or $VoiceId -eq '..') {
        throw 'VoiceId must be a folder name under SampleRoot.'
    }

    $sampleDir = Join-Path (Resolve-Path -LiteralPath $SampleRoot).ProviderPath $VoiceId
    if (-not (Test-Path -LiteralPath $sampleDir -PathType Container)) {
        throw "VoiceId folder does not exist: $sampleDir"
    }

    $referenceAudioPath = Join-Path $sampleDir 'reference.mp3'
    $referenceTextPath = Join-Path $sampleDir 'reference.txt'
    if ((Test-Path -LiteralPath $referenceAudioPath -PathType Leaf) -and
        (Test-Path -LiteralPath $referenceTextPath -PathType Leaf)) {
        $sample = Get-Item -LiteralPath $referenceAudioPath
        $referenceText = [System.IO.File]::ReadAllText($referenceTextPath).Trim()
    }
    else {
        $referenceText = '你好，这是我的声音样本，请记住我说话的声音和语气。'
        $samples = @(Get-ChildItem -LiteralPath $sampleDir -File |
            Where-Object {
                $_.Extension -eq '.mp3' -and
                -not $_.Name.StartsWith('._') -and
                $_.BaseName.EndsWith($referenceText, [StringComparison]::Ordinal)
            })
        if ($samples.Count -ne 1) {
            throw "Expected one long reference sample for $VoiceId; found $($samples.Count)."
        }
        $sample = $samples[0]
    }
    if ([string]::IsNullOrWhiteSpace($referenceText)) {
        throw "Reference text is empty for $VoiceId."
    }
    $audio = [System.IO.File]::ReadAllBytes($sample.FullName)
    if ($audio.Length -eq 0) {
        throw "Reference audio is empty: $($sample.FullName)"
    }

    $token = Get-EnvValue -Key 'VOICE_API_TOKEN' -File $EnvFile
    if ([string]::IsNullOrWhiteSpace($token)) {
        throw 'VOICE_API_TOKEN is missing. Set the environment variable or pass -EnvFile.'
    }
    if ([string]::IsNullOrWhiteSpace($ApiUrl)) {
        $ApiUrl = Get-EnvValue -Key 'VOICE_CLONE_BASE_URL' -File $EnvFile
    }
    if ([string]::IsNullOrWhiteSpace($ApiUrl)) {
        $ApiUrl = 'https://voiceclone.lenwellhome.com'
    }
    $ApiUrl = $ApiUrl.TrimEnd('/')
    if (-not $ApiUrl.StartsWith('https://', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'ApiUrl must use HTTPS.'
    }

    $hashInput = [System.IO.MemoryStream]::new()
    try {
        $hashInput.Write($audio, 0, $audio.Length)
        $textBytes = [Text.Encoding]::UTF8.GetBytes($referenceText)
        $hashInput.Write($textBytes, 0, $textBytes.Length)
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $hash = [BitConverter]::ToString($sha.ComputeHash($hashInput.ToArray())).Replace('-', '').ToLowerInvariant()
        }
        finally {
            $sha.Dispose()
        }
    }
    finally {
        $hashInput.Dispose()
    }
    $referenceId = 'sdk-voice-clone-' + $hash.Substring(0, 16)

    $client = [System.Net.Http.HttpClient]::new()
    $client.Timeout = [TimeSpan]::FromMinutes(3)
    $client.DefaultRequestHeaders.Authorization =
        [System.Net.Http.Headers.AuthenticationHeaderValue]::new('Bearer', $token)
    $client.DefaultRequestHeaders.Add('x-owner-id', 'iot-voice-clone-test')
    try {
        $form = [System.Net.Http.MultipartFormDataContent]::new()
        $form.Add([System.Net.Http.StringContent]::new($referenceId), 'id')
        $form.Add([System.Net.Http.StringContent]::new($referenceText), 'text')
        $audioPart = [System.Net.Http.ByteArrayContent]::new($audio)
        $audioPart.Headers.ContentType =
            [System.Net.Http.Headers.MediaTypeHeaderValue]::new('audio/mpeg')
        $form.Add($audioPart, 'audio', $sample.Name)

        Write-Host "VoiceId: $VoiceId"
        Write-Host "Sample: $($sample.FullName)"
        Write-Host "ReferenceId: $referenceId"
        $registered = Send-VoiceRequest -Client $client -Url "$ApiUrl/v1/references/add" -Content $form
        try {
            if (-not $registered.IsSuccessStatusCode -and [int]$registered.StatusCode -ne 409) {
                throw "Reference registration failed: $(Get-ResponseError $registered)"
            }
        }
        finally {
            $registered.Dispose()
        }

        $payload = @{
            text = $Text
            reference_id = $referenceId
            format = 'wav'
            streaming = $false
            use_memory_cache = 'on'
            max_new_tokens = 384
            chunk_length = 200
        } | ConvertTo-Json -Compress
        $json = [System.Net.Http.StringContent]::new(
            $payload, [Text.Encoding]::UTF8, 'application/json')
        $synthesized = Send-VoiceRequest -Client $client -Url "$ApiUrl/v1/tts" -Content $json
        try {
            if (-not $synthesized.IsSuccessStatusCode) {
                throw "Voice synthesis failed: $(Get-ResponseError $synthesized)"
            }
            $wav = $synthesized.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
        }
        finally {
            $synthesized.Dispose()
        }
    }
    finally {
        $client.Dispose()
    }

    if ($wav.Length -lt 44 -or
        [Text.Encoding]::ASCII.GetString($wav, 0, 4) -ne 'RIFF' -or
        [Text.Encoding]::ASCII.GetString($wav, 8, 4) -ne 'WAVE') {
        throw 'Voice service did not return a valid WAV file.'
    }

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $safeVoiceId = [regex]::Replace($VoiceId, '[<>:"/\\|?*\x00-\x1f]', '_').Trim(' ', '.')
        $safeText = [regex]::Replace($Text, '[<>:"/\\|?*\x00-\x1f]', '_').Trim(' ', '.')
        if ($safeText.Length -gt 80) {
            $safeText = $safeText.Substring(0, 80).Trim(' ', '.')
        }
        $OutputPath = Join-Path $PSScriptRoot "output\$safeVoiceId-$safeText.wav"
    }
    $destination = [System.IO.Path]::GetFullPath($OutputPath)
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($destination)) | Out-Null
    if (Test-Path -LiteralPath $destination) {
        $base = [System.IO.Path]::GetFileNameWithoutExtension($destination)
        $folder = [System.IO.Path]::GetDirectoryName($destination)
        $number = 2
        do {
            $destination = Join-Path $folder ("{0} ({1}).wav" -f $base, $number)
            $number++
        } while (Test-Path -LiteralPath $destination)
    }
    $stream = [System.IO.File]::Open($destination, [System.IO.FileMode]::CreateNew)
    try {
        $stream.Write($wav, 0, $wav.Length)
    }
    finally {
        $stream.Dispose()
    }
    Write-Host "Output: $destination ($($wav.Length) bytes)"
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
