<#
Windows (x64/ARM64) port of ota_v4/spilt_firmware: full_ci1306_V*.bin -> ota_ci1306_V*.bin.
Same layout as main.c: header + 4K, partition table + 4K, then user_code2, asr, dnn,
voice and user_file. Needs only Windows PowerShell 5.1; no C compiler.

  powershell -ExecutionPolicy Bypass -File .\make_ota.ps1 .\full_ci1306_V3.0.29.bin
#>
param(
    [Parameter(Mandatory = $true)][string]$Firmware,
    [string]$OutputDir = ''
)
$ErrorActionPreference = 'Stop'
# Windows PowerShell 5.1 leaves $PSScriptRoot empty in param defaults under -File.
if (-not $OutputDir) {
    $scriptPath = $MyInvocation.MyCommand.Path
    $OutputDir = $(if ($scriptPath) { Split-Path -Parent $scriptPath } else { (Get-Location).Path })
}

$HeadOffset = 0x6000        # HEAD_LENGTH: boot area before the partition table
$Page = 4096                # PACK_LENGTH_4K
$TableLength = 278          # sizeof(partition_table_t), #pragma pack(1)

$inputPath = (Resolve-Path -LiteralPath $Firmware).Path
$src = [IO.File]::ReadAllBytes($inputPath)
if ($src.Length -lt $HeadOffset + $TableLength) { throw "firmware too small: $inputPath" }

$table = New-Object byte[] $TableLength
[Array]::Copy($src, $HeadOffset, $table, 0, $TableLength)
function Get-U32([int]$offset) { return [BitConverter]::ToUInt32($table, $offset) }

# Status byte offsets: user_code1, user_code2, asr, dnn, voice, user_file.
$statusUserCode1 = 182; $statusUserCode2 = 199
$dataStatuses = @(216, 233, 250, 267)
$formatVersion = $table[161]

# get_partition_table_crc: byte sum with the statuses forced to their packed values.
$sumTable = [byte[]]$table.Clone()
$sumTable[$statusUserCode1] = 0xF0
$sumTable[$statusUserCode2] = $(if ($formatVersion -eq 1) { 0xF0 } else { 0xFF })
foreach ($o in $dataStatuses) { $sumTable[$o] = 0 }
$sum = 0
for ($i = 0; $i -lt $TableLength - 2; $i++) { $sum = ($sum + $sumTable[$i]) -band 0xFFFF }
$table[276] = [byte]($sum -band 0xFF)
$table[277] = [byte](($sum -shr 8) -band 0xFF)

# Mark every OTA partition as pending (ota_partition_enable = {1,1,1,1,1}).
$table[$statusUserCode1] = 0xF0
$table[$statusUserCode2] = 0xFF
foreach ($o in $dataStatuses) { $table[$o] = 0xFF }

function Get-Crc16Ccitt([byte[]]$data) {
    $crc = 0
    foreach ($b in $data) {
        $crc = $crc -bxor ([int]$b -shl 8)
        for ($k = 0; $k -lt 8; $k++) {
            if ($crc -band 0x8000) { $crc = (($crc -shl 1) -bxor 0x1021) -band 0xFFFF }
            else { $crc = ($crc -shl 1) -band 0xFFFF }
        }
    }
    return $crc
}

$names = @('user_code2', 'asr', 'dnn', 'voice', 'user_file')
$body = New-Object System.Collections.Generic.List[byte]
$body.Add(0); $body.Add([byte]$names.Count)
foreach ($n in $names) { $body.AddRange([Text.Encoding]::ASCII.GetBytes($n)) }
$bodyBytes = $body.ToArray()
$crc = Get-Crc16Ccitt $bodyBytes
$header = New-Object System.Collections.Generic.List[byte]
$header.Add([byte](($bodyBytes.Length -shr 8) -band 0xFF)); $header.Add([byte]($bodyBytes.Length -band 0xFF))
$header.AddRange($bodyBytes)
$header.Add([byte](($crc -shr 8) -band 0xFF)); $header.Add([byte]($crc -band 0xFF))
if ($header.Count -gt $Page) { throw 'OTA header exceeds 4K' }

$userCode2 = Get-U32 187; $asr = Get-U32 204; $dnn = Get-U32 221
$voice = Get-U32 238; $userFile = Get-U32 255; $userFileSize = Get-U32 259
# The comma operator binds tighter than '-', so every size is parenthesized.
$segments = @(
    @('user_code2', $userCode2, ([int64]$asr - $userCode2)),
    @('asr', $asr, ([int64]$dnn - $asr)),
    @('dnn', $dnn, ([int64]$voice - $dnn)),
    @('voice', $voice, ([int64]$userFile - $voice)),
    @('user_file', $userFile, [int64]$userFileSize)
)
foreach ($s in $segments) {
    if ($s[2] -le 0 -or $s[1] + $s[2] -gt $src.Length) {
        throw ("invalid partition {0}: offset=0x{1:x} size=0x{2:x} file=0x{3:x}" -f $s[0], $s[1], $s[2], $src.Length)
    }
}

$leaf = Split-Path -Leaf $inputPath
if ($leaf.StartsWith('full_')) { $leaf = $leaf.Substring(5) }
$outputPath = Join-Path $OutputDir ("ota_" + $leaf)

$out = New-Object IO.MemoryStream
$out.Write($header.ToArray(), 0, $header.Count)
$out.Write((New-Object byte[] ($Page - $header.Count)), 0, $Page - $header.Count)
$out.Write($table, 0, $TableLength)
$out.Write((New-Object byte[] ($Page - $TableLength)), 0, $Page - $TableLength)
foreach ($s in $segments) {
    $out.Write($src, [int]$s[1], [int]$s[2])
    Write-Host ("{0,-10} offset=0x{1:x6} size=0x{2:x6}" -f $s[0], $s[1], $s[2])
}
[IO.File]::WriteAllBytes($outputPath, $out.ToArray())

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
Write-Host ("input : {0}" -f $inputPath)
Write-Host ("output: {0} ({1} bytes)" -f $outputPath, $out.Length)
Write-Host ("sha256: {0}" -f $hash)
