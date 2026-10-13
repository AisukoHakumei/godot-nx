# 06 — Audio API: audout vs audren for Godot's `AudioDriver`

Ticket: "Audio API: audout vs audren for Godot's AudioDriver". Target: Godot 4.7.2-stable on devkitA64 + libnx (installed: `libnx 4.12.0-1`, `switch-examples 20260201-1`). Researched 2026-10-13 against the installed libnx headers, libnx `master` sources and git history, the installed switch-examples, and the devkitPro SDL2 / RetroArch / Stary2001 ports. Every claim is tagged `[Sn]` to the Sources list at the end.

## Summary

Use **audout** (`audout:u`, `audoutAppendAudioOutBuffer` + `audoutWaitPlayFinish`) for the Godot driver, with audren kept as a documented fallback.
Godot needs exactly what audout exposes and nothing more: one 48 kHz / 2-channel / Int16 stream (`audoutGetSampleRate` "48000Hz", `audoutGetChannelCount` "2 channels", `audoutGetPcmFormat` "Int16") fed by one thread [S1]. `audio_server_process` already hands us int32 frames whose top 16 bits are the sample, so the conversion is a `>> 16` copy, as PulseAudio and the 3.x Switch port do [S14][S20].
The submit/wait loop is 2 IPC calls per buffer (append, then wait on the buffer-release event) versus audren's `audrvUpdate` (one `RequestUpdate` IPC) on every 5 ms renderer frame plus cache flushes, mempool attach and voice/wavebuf bookkeeping in a wrapper whose own source still carries "todo" comments [S2][S3][S5].
The one historical reason ports fled audout — stuttery audio after sleep/resume, which drove SDL2 (2024) and earlier Cpasjuste work to audren — was a libnx bug (`audoutWaitPlayFinish` missing buffer-release events) fixed in libnx 4.10.0 (PR #683, July 2025) [S6][S7][S10][S11]. Our installed 4.12.0 contains the fix [S4][S6].
Alignment is trivial: `memalign(0x1000, …)` and a 0x1000-rounded `buffer_size`, no `armDCacheFlush` needed by any audout user we found [S1][S8][S18][S19].
Risks to retire on hardware: audout's queue depth cap and internal DSP latency are undocumented (ports use 2–5 queued buffers) [S9][S18][S19]; and audio-device changes (Bluetooth) are untested on either path — the only report is an open issue on the audren-based 3.5 port where the wait loop hangs [S21]. So the loop below always waits with a timeout and never blocks while holding `AudioDriver::lock()`.
Buffer size for ~20–40 ms: 3 buffers × 480 frames (10 ms each; data_size 1920 B inside one 4096 B page) ⇒ 20–30 ms queued ahead of the DSP; use 4 × 480 for 30–40 ms if dropouts appear.

## Comparison table

| Aspect | audout (`audout.h`) | audren + audrv (`audren.h`, `audio/driver.h`) |
|---|---|---|
| What it is | Direct PCM output stream to the default device (`"DeviceOut"`), opened by `audoutInitialize()` with `DEFAULT_SAMPLE_RATE 0xBB80` (48000) and channel count `0x00020000` [S1][S2] | A DSP-side mixer ("audio renderer") with voices, mixes, sinks, effects; libnx `audrv*` is a "high level wrapper around audren" [S3][S5] |
| Sample rate | Whatever the device returns from `OpenAudioOut`; header documents 48000 Hz only [S1]. The old SDL2 audout backend took `audoutGetSampleRate()` as-is; the audren commit's stated motive was to "allow freqs other than 48khz" [S19][S12] | Output rate 32 kHz or 48 kHz (`AudioRendererOutputRate_*`); each voice has its own `sample_rate` and `pitch`, so the renderer resamples [S3][S5] |
| Format | Device-reported `PcmFormat` (Int16 on real hardware per header; RetroArch asserts "audout is PCM_INT16 only") [S1][S18] | `audrvVoiceInit` accepts `PcmFormat_Int16` and `PcmFormat_Adpcm` only (RetroArch comment; Float rejected) [S17] |
| Channels | 2 [S1] | Final mix `num_mix_buffers` channels; device sink takes up to 6 inputs (`inputs[6]`) [S3][S17] |
| Buffer alignment | `AudioOutBuffer.buffer` and `buffer_size` "aligned to 0x1000 bytes"; `data_size` = bytes to play; the struct's address is the identifier the service returns on release (`BufferClientPtr`), so the struct must stay at a fixed address while queued [S1][S9] | Mempool: `AUDREN_MEMPOOL_ALIGNMENT 0x1000`; wave data `AUDREN_BUFFER_ALIGNMENT 0x40`; in/out param buffers 0x1000 / 0x10 [S3]. Data written to the pool must be `armDCacheFlush`ed (every example and port does it) [S15][S16][S17][S20] |
| Buffer count | Max queued count undocumented on switchbrew ("No maximum queued-buffer count is documented") [S9]; `audoutGetAudioOutBufferCount` [4.0.0+] reports current depth [S1]. Ports use 2 (SDL2 audout), 5 (RetroArch), 1 (playtone example) [S19][S18][S8] | Up to 4 wave buffers per voice (`AudioRendererWaveBuf wavebufs[4]`) [S3]; ports use 2 (SDL2, Stary2001) or 5 (RetroArch) [S16][S20][S17] |
| Submit / wait | `audoutAppendAudioOutBuffer(buf)`; `audoutWaitPlayFinish(&released, &n, timeout_ns)` = `eventClear` + poll `GetReleasedAudioOutBuffer`, wait on the buffer event only if nothing released, then poll again; `audoutPlayBuffer` = append + wait forever [S2] | `audrvVoiceAddWaveBuf` → `audrvUpdate` (IPC `RequestUpdate`) → `audrenWaitFrame()` = `eventWait(frame_event, UINT64_MAX)`; wavebuf state only changes on `audrvUpdate` [S2][S5] |
| Timing granularity | Buffer-release event; DSP consumption granularity undocumented [S9] | Renderer frame = 5 ms = 240 samples @48 kHz (`AUDREN_TIMER_PERIOD_MS`, `AUDREN_SAMPLES_PER_FRAME_48KHZ`) [S3] |
| Latency | ≈ bytes queued ahead + undocumented service/DSP latency; measurable with `audoutGetAudioOutPlayedSampleCount` [4.0.0+] [S1][S9] | ≈ queued wavebufs + ≥1 frame before a wavebuf reaches `Playing` (SDL2/Stary2001 spin until it does) + undocumented DSP latency; measurable with `audrvVoiceGetPlayedSampleCount` [S16][S20][S5] |
| Threading | One global autoclear `Event` (`g_audoutBufferEvent`) and `eventClear` inside `audoutWaitPlayFinish` ⇒ exactly one waiter thread [S2]. Precedent: feeder thread at priority `0x2B` via `svcSetThreadPriority` [S20]; SDL2 "increase audio thread priority" [S11] | `AudioDriver` struct has no locking; all state lives in the param buffer, so one thread owns it (RetroArch: "the renderer and the wave buffer states belong to the thread that writes") [S17]. `audrenWaitFrame` has no timeout; libnx 4.10 added `audrenGetFrameEvent()` so callers can `eventWait` with one [S2][S6][S13] |
| Sleep/resume | Pre-4.10 bug: after sleep all queued buffers release at once, autoclear event swallowed extra signals, playback starved ("distorted audio … dropped samples"); fixed by poll-first in PR #683 [S10] | Worked across suspend, which is why SDL2 reverted to it in 2024 [S11][S12] |
| Device change / Bluetooth | Untested. The 3.5 port's reporter speculates audout "follow[s] the system's output device" but "could not verify" [S21] | Open issue: with a Bluetooth sink the wavebuf never reaches `Playing`; unbounded spin under `lock()` hangs the whole app [S21] |
| libnx maintenance | `audout.c`: 2018 initial → 2020 (all IAudioOut cmds, aruid fix) → 2025-07-25 (#636 audout:a/d, #683 event fix). Not deprecated; only the applet/debug variants `audout:a`/`audout:d` were removed by HOS 11.0.0 [S6][S1][S9] | `audren.c`: 2018 → 2019 new-ipc → 2025-08-16 (#689 frame event). `audio/driver.c`: 2018 initial, 2021 alloc hook, nothing since; contains "todo: effects, splitters", "todo: this is supposed to be more complex", and revision selection capped at REV4 with REV5/6 commented out [S6][S3][S5][S2] |
| HOS version gates | Core cmds [1.0.0+]; `GetAudioOutBufferCount`/`PlayedSampleCount`/`Flush` [4.0.0+]; volume [6.0.0+] [S1] | [1.0.0+]; revision picked by `hosversionGet()` [S2] |
| Fit for Godot | Exact: one int16 stereo 48 kHz stream from one mixer thread | Over-featured: 24-voice renderer used as a 1-voice FIFO; adds cache flush, mempool, 5 ms polling |

## Minimal loop sketch (audout)

Derived from the installed headers [S1][S4], libnx `audout.c` [S2], the playtone example [S8], the SDL2 audout backend [S19], RetroArch's audout driver [S18], and Godot's `AudioDriver` contract [S14][S22]. 48 kHz stereo int16: 4 bytes/frame; 480 frames = 10 ms = 1920 B; 3 buffers in flight ⇒ 20–30 ms queued (between 2 and 3 buffers ahead of the DSP at any instant) plus the undocumented service latency.

```c
#include <switch.h>
#include <malloc.h>
#include <string.h>

#define NX_RATE       48000
#define NX_CH         2
#define NX_BUF_COUNT  3                         /* 3 x 10 ms -> 20..30 ms queued; 4 -> 30..40 ms */
#define NX_BUF_FRAMES 480                       /* 10 ms; multiple of the 240-sample (5 ms) DSP frame */
#define NX_DATA_BYTES (NX_BUF_FRAMES * NX_CH * sizeof(int16_t))      /* 1920 */
#define NX_BUF_BYTES  ((NX_DATA_BYTES + 0xFFF) & ~(u64)0xFFF)        /* 4096: one 0x1000 page */
#define NX_WAIT_NS    100000000ULL              /* 100 ms: never block forever (sleep / device change) */

static AudioOutBuffer g_bufs[NX_BUF_COUNT];     /* fixed addresses: the service identifies buffers by pointer */
static int32_t        g_mix[NX_BUF_FRAMES * NX_CH]; /* audio_server_process() output */

/* AudioDriver::init() */
static Error nx_audio_init(void) {
    Result rc = audoutInitialize();                                  /* opens "DeviceOut" @48000/2ch, registers buffer event */
    if (R_FAILED(rc)) return ERR_CANT_OPEN;
    if (audoutGetSampleRate() != NX_RATE || audoutGetChannelCount() != NX_CH ||
        audoutGetPcmFormat() != PcmFormat_Int16) { audoutExit(); return ERR_UNAVAILABLE; }

    for (int i = 0; i < NX_BUF_COUNT; i++) {
        void *p = memalign(0x1000, NX_BUF_BYTES);                    /* 0x1000-aligned, 0x1000-sized */
        if (!p) { audoutExit(); return ERR_OUT_OF_MEMORY; }
        memset(p, 0, NX_BUF_BYTES);
        g_bufs[i].next = NULL; g_bufs[i].buffer = p; g_bufs[i].buffer_size = NX_BUF_BYTES;
        g_bufs[i].data_size = NX_DATA_BYTES; g_bufs[i].data_offset = 0;
    }
    rc = audoutStartAudioOut();
    if (R_FAILED(rc)) { audoutExit(); return ERR_CANT_OPEN; }
    for (int i = 0; i < NX_BUF_COUNT; i++)                           /* prime the queue with silence */
        audoutAppendAudioOutBuffer(&g_bufs[i]);
    /* mix_rate = NX_RATE; speaker_mode = SPEAKER_MODE_STEREO; then start the feeder thread
       (precedent: svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B) on that thread) */
    return OK;
}

/* feeder thread body; `active` is set by AudioDriver::start() */
static void nx_audio_thread(void *ud) {
    while (!exit_requested) {
        AudioOutBuffer *released = NULL; u32 n = 0;
        Result rc = audoutWaitPlayFinish(&released, &n, NX_WAIT_NS); /* poll-first, then wait on event */
        if (R_FAILED(rc) || n == 0 || released == NULL)
            continue;                                                /* timeout: output stopped; retry, nothing is held */

        lock();
        if (active) audio_server_process(NX_BUF_FRAMES, g_mix);      /* int32, sample in the high 16 bits */
        else        memset(g_mix, 0, sizeof g_mix);
        unlock();

        int16_t *dst = (int16_t *)released->buffer;
        for (unsigned i = 0; i < NX_BUF_FRAMES * NX_CH; i++) dst[i] = (int16_t)(g_mix[i] >> 16);
        released->data_size = NX_DATA_BYTES;
        audoutAppendAudioOutBuffer(released);                        /* requeue the buffer we just filled */
    }
}

/* AudioDriver::finish() */
static void nx_audio_finish(void) {
    exit_requested = true; /* join thread */
    audoutStopAudioOut();                                            /* "waits for audio playback to finish" */
    audoutExit();
    for (int i = 0; i < NX_BUF_COUNT; i++) free(g_bufs[i].buffer);
}
```

Notes on the sketch:
- `audoutGetReleasedAudioOutBuffer` returns one pointer per call (`sizeof(*Buffer)` output buffer), so if several buffers were released at once (sleep/resume) the next `audoutWaitPlayFinish` returns immediately from its poll — this is exactly the 4.10.0 fix, and the loop above relies on it [S2][S10].
- Godot's existing drivers derive `buffer_frames = closest_power_of_2(output_latency_ms * mix_rate / 1000)` from the `audio/driver/output_latency` setting (default 15 ms ⇒ 512 frames @48 kHz) [S14][S23]. Using that formula instead of 480 gives 3 × 512 = 21–32 ms; both land in the 20–40 ms band. Keep the 0x1000 rounding of `buffer_size` regardless of frame count.
- Force `mix_rate = 48000` (ignore `audio/driver/mix_rate` or warn) because audout reports a device rate and does no resampling [S1][S12].
- `AudioServer` mixes internally in fixed 512-frame steps and `_driver_process` copies out whatever the driver asks for, so 480-frame driver buffers are fine [S22].

Equivalent audren skeleton (fallback), from `audren-simple` [S15] and the 3.5 port [S20]: `audrenInitialize(&cfg{48kHz,24 voices,1 sink,1 mix,2 mix buffers})` → `audrvCreate(&drv,&cfg,2)` → `memalign(0x1000, pool)` → `audrvMemPoolAdd/Attach` → `audrvDeviceSinkAdd(AUDREN_DEFAULT_DEVICE_NAME,2,{0,1})` → `audrvUpdate` → `audrenStartAudioRenderer` → `audrvVoiceInit(0,2,PcmFormat_Int16,48000)` + `SetDestinationMix(FINAL_MIX)` + mix factors; per buffer: memcpy into pool, `armDCacheFlush`, `audrvVoiceAddWaveBuf`, `audrvUpdate`, `eventWait(audrenGetFrameEvent(), timeout)` until the wavebuf is `Done`/`Free` again — with a bounded wait, unlike the 3.5 port [S20][S21].

## What other ports chose

| Port | API | Buffers | Why / notes |
|---|---|---|---|
| switch-examples `audio/playtone`, `audio/echo` [S8] | audout | 1 buffer, 1/30 s (1600 frames), `memalign(0x1000)`, `audoutPlayBuffer` | Simplest possible demo; blocks until played |
| switch-examples `audio/audren-simple`, `audio/hwopus-decoder` [S15] | audren/audrv | mempool 0x1000, `armDCacheFlush`, `audrvUpdate` per main-loop iteration | Demonstrates voices/wavebufs; hwopus feeds decoded Opus |
| switch-examples `audio/sdl2-audio` [S8] | SDL2 → audren | — | Goes through the devkitPro SDL2 backend below |
| devkitPro SDL2 `switch-sdl-2.28` [S11][S16] | audren | 2 wavebufs of `spec.samples`, spin `audrvUpdate`+`audrenWaitFrame` until `Playing` | History: Cpasjuste "switch: use audren audio — fix suspend, allow freqs other than 48khz" [S12]; Wohlstand "Switch Audio: Use AUDOUT" ("Audio Out is the direct output of the audio stream without conversions") [S13]; rsn8887 PR #64 (2024-03-24) reverted to audren because audout was "scratchy/stuttery after suspend" [S11]. The audout variant survives as branch `switch-sdl-2.28-audout` (2 buffers, `audoutWaitPlayFinish(UINT64_MAX)`) [S19]. The libnx #683 author argued the fix "could let the SDL2 backend return to audout", and it was verified in sys-tune [S10] |
| RetroArch (libnx) [S17][S18][S24] | both drivers; default `switch_audren` | audout driver: 5 buffers, 1/30 s chunks, bounded `audoutWaitPlayFinish(100 ms)`; audren driver: 5 wavebufs sized from the latency setting, `eventWait(frame event, 256 ms)` | `configuration.c`: default on libnx is `"switch_audren"`; the audout driver's libnx start/stop paths are marked "TODO/FIXME" [S24][S18] |
| Stary2001/godot (3.5.x `platform/switch/audio_driver_switch.cpp`) [S20] | audren | 2 wavebufs of `closest_power_of_2(latency*mix_rate/1000)` frames, thread prio 0x2B | Same shape as SDL2's audren backend; results of `audrenInitialize`/`audrvCreate` printed but unchecked; unbounded `while (state != Playing) audrvUpdate()` under `lock()` → open issue #40 (Bluetooth hang) [S21] |

Pattern: every port that picked audren did so either to survive sleep/resume on pre-4.10 libnx or because it copied SDL2. None cites a latency or quality advantage for audren. With libnx ≥ 4.10 the suspend reason is gone, and audout is the lighter path for a single fixed-format stream.

## Sources

- [S1] `/opt/devkitpro/libnx/include/switch/services/audout.h` (libnx 4.12.0-1): `AudioOutBuffer` field comments ("aligned to 0x1000 bytes"), `audoutGetSampleRate` "(48000Hz)", `audoutGetChannelCount` "(2 channels)", `audoutGetPcmFormat` "(Int16)", `audoutWaitPlayFinish` doc, version gates, "audout:a … Removed in [11.0.0]".
- [S2] libnx `nx/source/services/audout.c` and `nx/source/services/audren.c` at `master` — https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/services/audout.c , https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/services/audren.c (`DEFAULT_SAMPLE_RATE 0xBB80`, `_audoutInitialize`, `audoutWaitPlayFinish` poll-first logic, single `g_audoutBufferEvent`; `audrenWaitFrame` = `eventWait(…, UINT64_MAX)`, revision selection with REV5/6 commented out, `param.sample_count = 240` @48 kHz).
- [S3] `/opt/devkitpro/libnx/include/switch/services/audren.h` and `/opt/devkitpro/libnx/include/switch/audio/driver.h`: `AUDREN_TIMER_PERIOD_MS 5.0f`, `AUDREN_SAMPLES_PER_FRAME_48KHZ 240`, `AUDREN_MEMPOOL_ALIGNMENT 0x1000`, `AUDREN_BUFFER_ALIGNMENT 0x40`, `AudioRendererOutputRate_{32,48}kHz`, `wavebufs[4]`, `inputs[6]`, `AudioDriver` struct and `audrv*` API ("Audio driver (audren wrapper)").
- [S4] Installed package versions: `pacman -Q` → `libnx 4.12.0-1`, `switch-examples 20260201-1`.
- [S5] libnx `nx/source/audio/driver.c` at `master` — https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/audio/driver.c ("todo: effects, splitters", "todo: this is supposed to be more complex", `audrvUpdate` → `audrenRequestUpdateAudioRenderer`, `audrvCreate` input validation).
- [S6] libnx commit history via GitHub API (`/repos/switchbrew/libnx/commits?path=…`): `audout.c` 2018-02-17 "Initial audio support (#43)" … 2020-08-21 aruid fix … 2025-07-25 `0d1db8122` "add audout:a audout:d aud:a aud:d (#636)", `8d3dfbf37` "fix audout events being missed. (#683)"; `audren.c` 2018-08-13 … 2025-08-16 `d4a7fd1a3` "audren: expose frame event."; `audio/driver.c` 2018-08-13 `772c839c8` "Add audio/ (\"AudioDriver\"), a high level wrapper around audren", 2021-01-21 `076657fd3` alloc override. libnx `Changelog.md` (https://raw.githubusercontent.com/switchbrew/libnx/master/Changelog.md): Version 4.10.0 lists #683, #636, #689; 4.0.0 "Fixed audoutOpenAudioOut to properly pass aruid"; 3.2.0 "audout: Added all remaining IAudioOut commands."
- [S7] Same changelog/history: no deprecation notice exists for either `audout` or `audren` in libnx; the only removals are HOS-side (`audout:a`, `audout:d`, `audren:a`, `*:d` removed in 11.0.0) [S9].
- [S8] `/opt/devkitpro/examples/switch/audio/playtone/source/main.c`, `…/echo/source/main.c`, `…/sdl2-audio/source/main.c` (switch-examples 20260201): `SAMPLECOUNT = 48000/30`, `buffer_size = (data_size + 0xfff) & ~0xfff`, `memalign(0x1000, …)`, `audoutPlayBuffer`.
- [S9] switchbrew wiki "Audio services" — https://switchbrew.org/wiki/Audio_services : `AudioOutBuffer` layout, `AppendAudioOutBuffer` "input u64 BufferClientPtr", `RegisterBufferEvent` "signalled when a buffer is released", "No maximum queued-buffer count is documented", `Stop` "waits for audio playback to finish before returning", audren `RequestUpdate` revisions, "audout:a … removed with [11.0.0+]", "audout:d, audin:d, audrec:d, audren:d … removed with [11.0.0+]". switchbrew "Services API" (https://switchbrew.org/wiki/Services_API) lists `audout:u` and `audren:u` with no per-service session limit column, so the "audren has a limited number of sessions" remark in [S10] could not be verified from the wiki.
- [S10] libnx PR #683 "fix audout events being missed." — https://github.com/switchbrew/libnx/pull/683 (ITotalJustice, merged 2025-07-25 by fincs): sleep releases all queued buffers, autoclear event swallowed signals, playback starved; fix polls `audoutGetReleasedAudioOutBuffer` before waiting; author notes it could let SDL2 "return to audout instead of audren"; tested in sys-tune.
- [S11] devkitPro/SDL `switch-sdl-2.28` history of `src/audio/switch` — https://github.com/devkitPro/SDL/commits/switch-sdl-2.28/src/audio/switch and PR #64 "Fix regression with stuttery audio after suspend" — https://github.com/devkitPro/SDL/pull/64 (rsn8887, merged 2024-03-24: reverts "Switch Audio: Use AUDOUT" and "switch: audio: Fix deinitialization (#25)"; "we should stick with audren"). Also commits "switch: increase audio thread priority" and "Switch: Enable more audio formats with resampling (#12)".
- [S12] devkitPro/SDL commit `f9d0337` "switch: use audren audio — fix suspend, allow freqs other than 48khz" (Cpasjuste) — https://github.com/devkitPro/SDL/commit/f9d0337
- [S13] devkitPro/SDL commit `aeebe32e41b3e330c31e586950cf5c54d2baebdd` "Switch Audio: Use AUDOUT" (Wohlstand): "Audio Render is a second layer that converts audio stream to output. Audio Out is the direct output of the audio stream without conversions."; `NUM_BUFFERS 2`, `(size + 0xfff) & ~0xfff`, `memalign(0x1000, …)`, `audoutWaitPlayFinish(…, UINT64_MAX)`. libnx commit `d4a7fd1a3` "audren: expose frame event." adds `Event* audrenGetFrameEvent(void)` "signalled on new frame (autoclear=true)".
- [S14] Godot worktree (4.8-dev, interface identical on 4.7.2): `drivers/pulseaudio/audio_driver_pulseaudio.cpp` (`buffer_frames = Math::closest_power_of_2(tmp_latency * mix_rate / 1000)`, thread loop: `lock()` → `audio_server_process(buffer_frames, samples_in)` → `out[i] = samples_in[i] >> 16` → `unlock()`), `drivers/alsa/audio_driver_alsa.cpp` (same formula, `periods = 2`), `platform/android/audio_driver_opensl.cpp` (`BUFFER_COUNT = 2`, `buffer_size = 1024`, callback-driven); `main/main.cpp` `audio/driver/output_latency` default 15.
- [S15] `/opt/devkitpro/examples/switch/audio/audren-simple/source/main.c`, `…/hwopus-decoder/source/main.c`: `AudioRendererConfig{48kHz,24,0,1,1,2}`, `memalign(0x1000)`, `armDCacheFlush`, `audrvMemPoolAdd/Attach`, `audrvDeviceSinkAdd(AUDREN_DEFAULT_DEVICE_NAME,2,{0,1})`, `audrvUpdate` each loop.
- [S16] devkitPro/SDL `switch-sdl-2.28` `src/audio/switch/SDL_switchaudio.c` — https://raw.githubusercontent.com/devkitPro/SDL/switch-sdl-2.28/src/audio/switch/SDL_switchaudio.c (audren; 2 wavebufs; `while (state != Playing) { audrvUpdate(); audrenWaitFrame(); }`; `armDCacheFlush`).
- [S17] RetroArch `audio/drivers/switch_libnx_audren_audio.c` — https://raw.githubusercontent.com/libretro/RetroArch/master/audio/drivers/switch_libnx_audren_audio.c (`BUFFER_COUNT 5`; "audrenWaitFrame() is eventWait() with no timeout …"; `AUDREN_STALL_TIMEOUT_NS 256000000`; "The renderer and the wave buffer states belong to the thread that writes"; "audrvVoiceInit() takes PcmFormat_Int16 and PcmFormat_Adpcm and rejects everything else"; "The device sink takes at most six").
- [S18] RetroArch `audio/drivers/switch_audio.c` — https://raw.githubusercontent.com/libretro/RetroArch/master/audio/drivers/switch_audio.c (libnx: `BUFFER_COUNT 5`, 1/30 s `data_size`, `memalign(0x1000, capacity)`, `audoutWaitPlayFinish(…, 100 ms)` with lap cap, "audout is PCM_INT16 only", `TODO/FIXME - fix libnx codepath` on start/stop).
- [S19] devkitPro/SDL `switch-sdl-2.28-audout` `src/audio/switch/SDL_switchaudio.c` — https://raw.githubusercontent.com/devkitPro/SDL/switch-sdl-2.28-audout/src/audio/switch/SDL_switchaudio.c (audout; `NUM_BUFFERS` rotation, `aligned_size = (spec.size + 0xfff) & ~0xfff`, `audoutWaitPlayFinish(UINT64_MAX)` in `WaitDevice`, format taken from `audoutGetPcmFormat()`).
- [S20] Stary2001/godot `3.5.2-stable_switch` `platform/switch/audio_driver_switch.cpp` — https://raw.githubusercontent.com/Stary2001/godot/3.5.2-stable_switch/platform/switch/audio_driver_switch.cpp (audren; `buffer_size = closest_power_of_2(latency * mix_rate / 1000)`; 2 wavebufs; `svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B)`; `samples_out[i] = samples_in[i] >> 16`; unbounded `while (state != Playing) audrvUpdate()` under `lock()`; results printed, not checked).
- [S21] Stary2001/godot issue #40 "Bluetooth audio output hangs the whole app — unbounded wait in AudioDriverSwitch" — https://github.com/Stary2001/godot/issues/40 (open, no maintainer reply; driver reports `"AUDREN"`; reporter's audout speculation "could not verify"; lists six audren-based branches/forks, none using audout).
- [S22] Godot worktree `servers/audio/audio_driver.h` (4.8-dev) and upstream `servers/audio/audio_server.h` at tag `4.7.2-stable` (https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/servers/audio/audio_server.h, lines 48–105): `class AudioDriver` with `void audio_server_process(int p_frames, int32_t *p_buffer, bool p_update_mix_time = true)`, pure virtuals `init/start/get_mix_rate/get_speaker_mode/lock/unlock/finish`, `virtual float get_latency()`, `int _get_configured_mix_rate()`. Worktree `servers/audio/audio_server.cpp` `_driver_process`: int32 output built as `(vl < 0 ? -1 : 1) * (abs(vl) << 11)` from a 21-bit value, internal `buffer_size = 512` mix step.
- [S23] Godot worktree `core/math/math_funcs_binary.h` lines 98–108, `closest_power_of_2` returns whichever of `previous_power_of_2`/`next_power_of_2` is nearer: 15 ms × 48000 / 1000 = 720 → 512 frames (10.7 ms).
- [S24] RetroArch `configuration.c` — https://raw.githubusercontent.com/libretro/RetroArch/master/configuration.c (`case AUDIO_SWITCH: #if defined(HAVE_LIBNX) return "switch_audren";`) and `audio/audio_driver.c` driver table (`&audio_switch`, `&audio_switch_libnx_audren` under `HAVE_LIBNX`).
