# Wake-word audio corpus

`wake_audio_cases.json` is the source of truth for the test set. Each entry has
an ID, an expected class, a language group, and the exact text sent to the TTS
service. The generated WAV files live under:

```text
script/output/
  positive/{zh-CN,en-US,es-ES}/{嘉岚3.0,雷军}/
  negative/{zh-CN,en-US,es-ES}/{嘉岚3.0,雷军}/
```

Generate or resume the complete set:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\script\generate_wake_audio.ps1
```

Use `-Class positive` or `-Class negative`, `-Language zh-CN`, and/or
`-VoiceIds '雷军'` to narrow a run. Existing destination files are skipped.
`-DryRun` prints the intended paths without calling the service.

The language folders describe the *intended test language*. The cloning API
does not receive a language parameter, so it may pronounce Latin words using
the wrong accent. Listen to each file before treating it as a valid test case.
Do not use a positive file whose actual pronunciation differs from VITANA or
HI-VITANA, and do not use a negative file that accidentally contains either
target phrase.

On the device, a positive case passes only when it wakes consistently; a
negative case passes only when it stays asleep. Record the case ID, voice,
actual pronunciation, wake result, and serial log for every failure. Especially
retest `N-ZH-01` (米塔纳), `N-ZH-10` (被他拿), `N-ZH-13` (卧槽拿), and `N-EN-15`
(Color key), because those are reported or suspected confusions. A synthetic
corpus cannot prove perfect real-world behavior; follow up with real speakers,
room noise, playback volume/distance variation, and repeated trials.

To play the whole set through the PC speaker and capture the COM3 module log:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\script\test_wake_audio_pc.ps1
```

The runner plays each WAV twice by default. It waits for a wake result and AEC
recovery, or an eight-second no-result window, before advancing. Each run gets
its own `script/output/test_runs/<timestamp>/results.csv` and `serial.log`.
The first complete run and its findings are in
`script/output/test_runs/20260923_162345/summary.md`.
