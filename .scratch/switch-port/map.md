# Map: Godot 4.7 on Nintendo Switch via devkitPro

Label: wayfinder:map
Tracker: local markdown (`.scratch/switch-port/`, see `~/.claude/skills/setup-matt-pocock-skills/issue-tracker-local.md`)

## Destination

A design spec at `.scratch/switch-port/spec.md` for a `platform/switch` port of Godot `4.7.2-stable` (branch `switch-4.7`) built with devkitPro's devkitA64 + libnx, that someone can implement without further decisions. The spec's first milestone: a 2D Godot project with audio, Joy-Con/Pro Controller and touch input, loading a `.pck` from the SD card, running as a homebrew NRO on an emulator and on real hardware.

## Notes

- Domain: Godot engine platform ports (SCons `platform/<name>/detect.py`, `OS`, `DisplayServer`, `AudioDriver`, `Input`), libnx homebrew, devkitPro portlibs.
- Skills every session should consult: `grilling` + `domain-modeling` for decisions; `research` for AFK fact-finding; `prototype` for the hello-triangle ticket. Read `CONTEXT.md` first for vocabulary.
- Planning only: tickets resolve decisions. The sole exceptions are the two Task tickets (branch creation, portlibs install) and the one Prototype ticket, which exist to unblock decisions.
- Standing constraints settled while charting (2026-10-13):
  - Source: fork branches `switch-4.7` from upstream tag `4.7.2-stable`; master (4.8 dev) is not the target.
  - Homebrew only (libnx, modded console or emulator). Runtime/export template only; no editor on the console.
  - Renderer: Compatibility (GLES3) through devkitPro mesa (20.1 nouveau over libdrm_nouveau) + EGL only. No Vulkan (unavailable); deko3d `RenderingDevice` is out of scope.
  - Build: native SCons platform `switch` driving devkitA64. Bundled `thirdparty/` for everything Godot ships; portlibs only for the GPU stack and libnx needs.
  - Input: direct libnx HID, no SDL2. Audio: libnx `audout` or `audren`, picked by research.
  - Project data: `.pck` beside the NRO on `sdmc:/switch/<game>/` for the first milestone; romfs embedding is fog for the export preset.
  - Packaging: phase 1 is a script around `elf2nro`; an editor `EditorExportPlatform` is a later phase inside the spec.
  - First milestone lifecycle: docked/handheld resolution changes only. Sockets, sleep/resume are fog.
  - Testing: Eden emulator on the Windows host for iteration, real hardware (Atmosphère + nxlink) as ground truth.
  - Debugging: `nxlink` stdout redirection in scope; GDB stub is fog.
  - CI: a minimal build-only GitHub Actions job on the `devkitpro/devkita64` container, as a later phase of the spec.

## Decisions so far

<!-- one line per resolved ticket: [title](issues/NN-slug.md): gist -->
- [Emulator for fast iteration](issues/07-emulator-pick.md): Eden (v0.2.1+ from git.eden-emu.dev), run on the Windows host not WSL2; pass `-p "<arg>"` to get libnx argv; no emulator does nxlink (use `svcOutputDebugString` logs); emulators run NROs in application mode while hbmenu on hardware runs applet mode, so memory behaviour differs. Findings in [research/07-emulator.md](research/07-emulator.md).
- [GPU stack: devkitPro mesa, EGL, and Godot's GLES3 needs](issues/05-gpu-stack-mesa-egl.md): mesa 20.1 nouveau over libdrm_nouveau (not deko3d); GL 4.3 core / ES 3.1 via EGL 1.4 on `NWindow`; GL via `eglGetProcAddress` so glad works without dlopen, but Godot's `EGLManager` needs a Switch-specific replacement; risks: no program binaries, no ASTC/ETC2, timer-query stub, `eglQuerySurface` returns 0. Findings in [research/05-gpu-stack.md](research/05-gpu-stack.md).
- [libnx runtime vs Godot 4.7 core requirements](issues/04-libnx-runtime-for-godot-core.md): core links as-is; shims needed for 128 KiB pthread stacks (`PLATFORM_THREAD_OVERRIDE`), `CLOCK_MONOTONIC_RAW`, `hardware_concurrency()`=0, missing `sigaction`/`pipe`/`popen`/`mmap`/`dlopen`/`backtrace`; app mode ~3285 MiB vs applet pool 507 MiB; nxlink needs `setvbuf`. Findings in [research/04-libnx-runtime.md](research/04-libnx-runtime.md).
- [Prior art: existing Godot ports to Switch homebrew](issues/03-prior-art.md): lift Ozuba/godot `switch-platform` (4.7-rc, GLES3+EGL, `PLATFORM_THREAD_OVERRIDE` with 2 MiB stacks) as the skeleton minus its Vulkan/Mesa-26 dependency; take Stary2001's applet guard, hbmenu fileassoc/forwarder, and ASET exporter; nobody has proven Godot 4 GLES3 on stock `switch-mesa` 20.1. Findings in [research/03-prior-art.md](research/03-prior-art.md).
- [Audio API: audout vs audren for Godot's AudioDriver](issues/06-audio-api-pick.md): use `audout` (48 kHz/stereo/Int16, 3×480-frame 0x1000-aligned buffers); the sleep/resume bug that drove ports to `audren` is fixed in libnx ≥4.10; audren is fallback only. Findings in [research/06-audio-api.md](research/06-audio-api.md).

## Not yet specified

- **Editor export preset**: `EditorExportPlatform` for Switch (NACP title/author/version, icon, `elf2nro`, romfs embedding of the `.pck`). Sharpens once packaging phase 1 and project-data loading are decided.
- **Networking**: `socketInitialize`/`nifm` wiring into Godot's `NetSocket`, when the network stack enters a later milestone.
- **Sleep/resume and applet focus**: `appletHook` events (focus loss, suspend, performance mode) mapped to `MainLoop` notifications. Depends on the platform-layer shape.
- **3D milestone**: which GLES3 renderer features break on mesa 20.1 nouveau (no program binaries, no ASTC/ETC2, timer queries, TGSI-only shader compiler) once the 2D milestone runs. Depends on the hello-triangle prototype.
- **Memory budget**: with the runtime facts known (app pool ~3285 MiB, applet pool 507 MiB, heap via hbloader `OverrideHeap`), what the first milestone does when launched in applet mode: refuse with a message (Stary2001's guard), or run with reduced caches. Sharpens with the platform-layer shape.
- **Crash handling**: Atmosphère fatal reports, symbolization of aarch64 ELF backtraces, GDB stub over nxlink.
- **GDExtension**: no `dlopen` on homebrew; decide static-only policy and what that means for the export pipeline.
- **Extras**: rumble (`hidSendVibrationValues`), system language via `setGetSystemLanguage`, save data location, profiler over nxlink, CI job details.

## Out of scope

- Official NintendoSDK / NDA platform support: homebrew only, by decision while charting.
- Editor running on the Switch: runtime only.
- deko3d `RenderingDevice` (Forward+/Mobile renderers): a separate multi-month effort.
- Upstreaming `platform/switch` to godotengine/godot: upstream will not take a homebrew platform; this lives in the fork.
- C#/.NET (mono module): no runtime on homebrew.
