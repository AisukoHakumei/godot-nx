# Audio API: audout vs audren for Godot's AudioDriver

Type: research
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

Godot needs one 48 kHz stereo float-or-int16 output stream fed from its own mixer thread. Compare libnx `audout` and `audren`: supported formats, buffer sizes, latency, threading rules, maintenance status, and which the devkitPro examples and prior Switch ports use. Recommend one, with the minimal init/submit loop sketched from libnx headers.

## Answer

**Use `audout`** (`audoutAppendAudioOutBuffer` + `audoutWaitPlayFinish`), with `audren` documented as a fallback only.

- audout exposes exactly what Godot needs: one 48 kHz, 2-channel, Int16 stream fed by one thread; Godot's `_driver_process` int32 output converts with a `>> 16` copy.
- Two IPC calls per buffer versus audren's per-5 ms-frame `audrvUpdate` plus mempool/cache/voice bookkeeping in a wrapper that still carries "todo" comments.
- The historical reason ports fled audout (stutter after sleep/resume) was a libnx bug fixed in libnx 4.10.0 (PR #683, July 2025); the installed libnx is 4.12.0.
- Buffers: `memalign(0x1000, …)`, 3 × 480 frames (10 ms each) for roughly 20-30 ms queued; 4 × 480 if dropouts appear.
- Risks to retire on hardware: audout queue-depth cap and DSP latency are undocumented; Bluetooth/device-change behaviour untested on either path (the audren-based 3.5 port hangs there). The driver loop must wait with a timeout and never block while holding `AudioDriver::lock()`.

Findings: [research/06-audio-api.md](../research/06-audio-api.md) (also on branch `research/audio-api`, commit c9d18855c2).
