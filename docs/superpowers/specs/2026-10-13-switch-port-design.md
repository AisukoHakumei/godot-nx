# Godot 4.7 on Nintendo Switch homebrew: design

Date: 2026-10-13
Branch: `switch-4.7` (forked from upstream tag `4.7.2-stable`)
Status: approved design, awaiting implementation plan

## 1. Purpose

Ship the author's own 3D GDScript game as Nintendo Switch homebrew, built with
devkitPro's devkitA64 toolchain and libnx. This is a single-game port in a
private fork, not a general-purpose platform for other developers and not a
feasibility study.

### What the user stated

- Game: 3D, GDScript only. No GDExtension, no C#.
- Renderer: Compatibility (GLES3). The game already runs with acceptable
  visuals on Compatibility on desktop. Volumetric fog, screen-space
  reflections and sub-surface scattering are dropped on Switch.
- Performance bar: 720p at 30 fps handheld; docked the same or better.
- Platform features: one player; Joy-Con and Pro Controller; touch; rumble;
  save data on SD card; sleep/resume; docked/handheld switching.
- Networking: dropped for now.
- Distribution: NRO plus `.pck` in a folder on the SD card, launched from
  hbmenu first. An installable forwarder comes later.
- Test targets: a modded Switch running Atmosphère, and an emulator on the
  Windows host.
- Effort: whatever it takes for the end result.

### Assumptions

- Source is Godot 4.7.2-stable on the `switch-4.7` branch. The fork never
  targets upstream.
- Building happens in WSL2 Linux where the toolchain is already installed:
  devkitA64, libnx 4.12, switch-mesa 20.1.0-5, switch-glad, switch-libdrm_nouveau,
  switch-tools (elf2nro, nacptool, nxlink), SCons 4.11.
- Shipped builds run in application mode (title takeover, ~3.2 GiB pool).
  Applet mode (plain hbmenu launch, ~500 MiB shared pool) is not a design
  target for a 3D game.
- The editor never runs on the console. Export happens on the PC.
- No official NintendoSDK, no deko3d backend, no Vulkan in the first
  milestones (see section 3).

### Success criteria

The game's exported `.pck`, dropped beside the Switch NRO on the SD card,
boots on the real console and on the emulator, renders the 3D scenes correctly
under Compatibility at 720p30, plays audio, takes Joy-Con, Pro Controller and
touch input with rumble, saves to SD, and survives sleep/resume and docking.

## 2. Vocabulary

See `CONTEXT.md` at the repo root for the shared glossary (Switch port,
Homebrew, NRO, Template, Portlibs, GPU stack, Application mode / Applet mode,
Engine thread, Docked / Handheld).

## 3. GPU approach and alternatives considered

Research for this decision is in
`docs/superpowers/specs/2026-10-13-switch-port-research/`.

### Chosen: Compatibility renderer on the official switch-mesa 20.1 portlib

Godot's GLES3 renderer in an OpenGL ES 3.1 context created through EGL 1.4 on
the libnx `NWindow`, built the way Android builds it: system GLES headers,
no glad loader, statically linked `libGLESv2`, `libEGL`, `libglapi`,
`libdrm_nouveau`.

Everything the 3D path calls exists in this driver: uniform buffers, 2D
texture arrays, instancing, transform feedback, MSAA renderbuffers up to 8x,
float colour buffers, shadow samplers, `gl_FragDepth`, fence syncs, and
S3TC/RGTC/BPTC texture compression exposed to ES contexts.

Known gaps and their handling:

| Gap | Handling |
|---|---|
| No program binaries, so Godot's shader cache is a no-op; every boot recompiles every variant single-threaded | Accept. Add a warm-up scene that touches every material. Keep variant count low. |
| No native ETC2 or ASTC (mesa CPU-transcodes ETC2; Godot CPU-decodes ASTC) | Export with S3TC + BPTC only. Force the renderer's ETC2 flag off on Switch. |
| `eglQuerySurface` returns 0 for width/height (open devkitPro bug since 2019) | Size from `appletGetOperationMode()`: 1920x1080 docked, 1280x720 handheld. |
| Surface dimensions are fixed once configured | Destroy and recreate the EGL surface on dock change. |
| Multiview and `glTexImage3DMultisample` symbols absent | Compile-time guards in three `drivers/gles3` files, as the iOS exclusions do. |
| `discard` in fragment shaders crashed the Godot 3.x port (never diagnosed) | Test in phase 0. Fallback: avoid alpha scissor, use alpha blend or hash. |
| Nobody has run Godot 4.x on this driver | Phase 0 is the proof. |

The GL context lives in one self-contained unit (`GLManagerSwitch`) behind
the display server so a different GPU path can replace it later without
touching the rest of the platform.

### Rejected: Compatibility on the community Mesa 26 GL path

Newer driver with a working shader cache, but the devkitPro maintainers report
its GL path black-screens, it is not a portlib, and it changes weekly.

### Deferred: Mobile renderer on community NVK (Vulkan)

As of October 2026 community Mesa 26.2 builds (danfromtico/mesa-switch,
PalindromicBreadLoaf/nxvk) expose Vulkan 1.3/1.4 on Horizon with a real
`VK_KHR_swapchain` on an `NWindow`. Godot 4.5 Mobile has been shown booting on
it with glitches and stutter. Costs: cross-building Mesa 26 with Rust and the
NAK compiler, title takeover required, GPU hang fixes still landing, a GPL
fork to avoid, and on-device pipeline compilation at first boot. This is the
escape hatch if phase 0 fails, and a candidate second milestone otherwise.
Godot's Vulkan driver only needs `VK_KHR_surface` + `VK_NN_vi_surface` +
`VK_KHR_swapchain`, so the Godot-side glue is about 20 lines.

### Rejected: deko3d RenderingDevice backend

`RenderingDeviceDriver` has 138 pure virtuals; existing backends are 10 to 13
thousand lines. deko3d has no pipeline objects, no render passes, and no
runtime shader compiler (uam is GLSL-only, offline). Specialization constants
have no clean mapping. No engine has a deko3d backend. Estimated 7 to 9
thousand new lines and 3 to 5 months. Out of scope.

## 4. Architecture

A native SCons platform at `platform/switch/`, producing one export template
ELF that a post-build step turns into an NRO. Five units:

```
platform/switch/
  detect.py                 toolchain discovery, flags, module set, NRO post-build
  platform_config.h         platform defines
  platform_gl.h             GLES_API_ENABLED + <GLES3/gl32.h>
  platform_egl.h            <EGL/egl.h>, <EGL/eglext.h>
  platform_thread.h/.cpp    PLATFORM_THREAD_OVERRIDE: 2 MiB stacks
  godot_switch.cpp          main(): libnx init, 8 MiB engine thread, teardown
  os_switch.h/.cpp          OS_Switch : OS
  display_server_switch.*   DisplayServerSwitch
  gl_manager_switch.*       GLManagerSwitch (EGL on NWindow)
  joypad_switch.*           JoypadSwitch (HID pads, touch, rumble)
  audio_driver_switch.*     AudioDriverSwitch (audout)
  switch_logger.*           log to SD file + svcOutputDebugString
  tools/package.py          phase-1 packaging script
  tools/default_icon.jpg    256x256 NACP icon
```

### 4.1 OS layer: `OS_Switch : OS`

Derives from `OS` directly, not `OS_Unix`. `OS_Unix` drags in `dlopen`,
pipes, `fork`, `sigaction` and `uname`, none of which link on libnx; guarding
all of it costs more than owning a small class.

- Registers `FileAccessUnix` and `DirAccessUnix` for resources, user data and
  filesystem access. No pipe access, no network sockets.
- Ticks from `clock_gettime(CLOCK_MONOTONIC)`; `delay_usec` via `nanosleep`.
- `get_processor_count()` returns 3 (hbloader exposes cores 0 to 2).
- `execute`, `create_process`, `kill`, `is_process_running` return
  `ERR_UNAVAILABLE`. `open_dynamic_library` returns `ERR_UNAVAILABLE`.
- `get_locale()` from `setGetSystemLanguage` + `setMakeLanguage`.
- `get_name()` returns `"Switch"`. Feature tags: `switch`, `arm64`, `gles3`,
  `s3tc`, `bptc`. The `mobile` tag is not set; project setting overrides
  target `switch`.
- Paths: see 4.6.
- `alert()` logs and shows the full-screen message (4.2).

### 4.2 Display server: `DisplayServerSwitch`

Single window, no cursor, no clipboard, no IME. Advertised features:
`FEATURE_TOUCHSCREEN`, `FEATURE_VIRTUAL_KEYBOARD` (via the `swkbd` applet),
`FEATURE_SWAP_BUFFERS`. Owns `GLManagerSwitch` and `JoypadSwitch`.

Per-frame `process_events()`:
1. `appletMainLoop()`; if false, request quit.
2. Apply pending applet-hook flags (resize, focus, resume) on this thread.
3. `JoypadSwitch::process()` (pads, touch).
4. Dispatch Godot input events.

Resolution: 1920x1080 docked, 1280x720 handheld, from
`appletGetOperationMode()`. The viewport applies the project's stretch
settings. Rendering 3D at a lower internal resolution when docked is a
project `scaling_3d` setting, not platform code.

Dock/undock: the applet hook for `AppletHookType_OnOperationMode` sets a
dirty flag. On the next frame, from the engine thread: destroy the EGL
surface, `nwindowSetDimensions`, recreate the surface, `eglMakeCurrent`,
emit `WINDOW_EVENT_RESIZED`.

Sleep/resume: `OnFocusState` with focus lost maps to
`NOTIFICATION_APPLICATION_FOCUS_OUT` and pauses the audio thread. Focus
regained maps to `FOCUS_IN`, resumes audio, and recreates the EGL surface.
Assumption: GL resources survive sleep because process memory is retained;
if hardware disproves it, fall back to a full context recreate (Godot's
rasterizer supports this for Android).

Frame pacing: `eglSwapInterval(1)` by default; `window_set_vsync_mode`
toggles it. The project's `max_fps = 30` with vsync on gives an even 30.

Applet-mode guard: at startup, if `appletGetAppletType()` is not
`AppletType_Application` / `SystemApplication`, show the full-screen message
"Launch this game via title override (hold R on a title)" and exit cleanly.
Hard stop, not a warning.

Full-screen message: a minimal path that draws text with libnx's console
API (`consoleInit` on the default NWindow). It is only usable before EGL
owns the window, which covers both callers: the applet-mode guard and the
missing-pck error, both raised before the renderer initialises. After the
renderer is up, `OS::alert` only logs; no in-game dialog is attempted.

Error handling: EGL init failure aborts with a logged message. Surface
recreation failure retries once, then aborts. All applet events are logged
under `--verbose`.

### 4.3 GPU context: `GLManagerSwitch`

About 120 lines, lifted in spirit from Ozuba's first commits:

- `initialize()`: `eglGetDisplay(EGL_DEFAULT_DISPLAY)` → `eglInitialize` →
  `eglBindAPI(EGL_OPENGL_ES_API)` → choose config with `EGL_OPENGL_ES3_BIT`,
  RGBA8, depth 24, stencil 8 → `nwindowSetDimensions` → `eglCreateWindowSurface(nwindowGetDefault())`
  → `eglCreateContext` with `EGL_CONTEXT_CLIENT_VERSION 3` → `eglMakeCurrent`
  → `RasterizerGLES3::make_current(false)`.
- `resize()`: destroy surface, set dimensions, recreate surface, make current.
- `swap_buffers()`, `set_use_vsync()`, `release_current()`, `make_current()`.
- `get_window_size()` from operation mode.
- Before `eglInitialize`, release builds set `MESA_NO_ERROR=1`; debug builds
  may set `EGL_LOG_LEVEL=debug`, `NOUVEAU_MESA_DEBUG=1`.

Engine-side guards outside the platform directory:

- `drivers/gles3/rasterizer_gles3.cpp`, `render_scene_buffers_gles3.cpp`,
  `storage/texture_storage.cpp`: `HORIZON_ENABLED` guards so
  `glFramebufferTextureMultiviewOVR` and `glTexImage3DMultisample` are never
  referenced (same shape as the iOS exclusions).
- `drivers/gles3/storage/config.cpp`: `etc2_supported = false` under
  `HORIZON_ENABLED`.
- `drivers/gl_context/SCsub` already skips glad for unknown platforms; no
  change.

### 4.4 Threading

`platform_thread.h` under `PLATFORM_THREAD_OVERRIDE` (precedent:
`drivers/apple/thread_apple.cpp`) creates pthreads with a 2 MiB stack, since
libnx defaults to 128 KiB. `main()` runs on hbloader's 1 MiB thread, so it
initialises libnx services, spawns an 8 MiB engine thread that runs
`Main::setup` / `Main::start` / the main loop / `Main::cleanup`, joins it,
then deinitialises services. Render thread model defaults to
`rendering/driver/threads/thread_model = 1` (safe) until separate-thread
rendering is tested on hardware.

### 4.5 Input: `JoypadSwitch`

- `padConfigureInput(1, HidNpadStyleSet_NpadStandard)`,
  `padInitializeDefault(&pad)`: handheld, dual Joy-Con or Pro Controller merge
  into one pad. One Godot joypad device, index 0, fixed GUID and name
  `"Nintendo Switch Controller"`.
- Buttons: A/B/X/Y by physical position to Godot's `JOY_BUTTON_A/B/X/Y`
  (Godot handles Nintendo layout swap), L/R → shoulders, ZL/ZR → trigger axes
  (0 or 1), Plus → START, Minus → BACK, stick clicks, D-pad to Godot D-pad
  buttons. Sticks: libnx ±32767 normalised to ±1.0.
- Rumble: `hidInitializeVibrationDevices` for the active style; Godot's
  `joy_start_vibration(weak, strong, duration)` maps to low/high amplitude
  `HidVibrationValue` sent with `hidSendVibrationValues`; stop sends zeros.
- Touch: `hidInitializeTouchScreen`; per frame `hidGetTouchScreenStates`;
  finger ids map to Godot touch indices; emit `InputEventScreenTouch` on
  begin/end and `InputEventScreenDrag` on move. Coordinates are in the
  1280x720 handheld space, scaled to the current window size.
- Virtual keyboard: `swkbdCreate`/`swkbdShow` from the display server; the
  result string is injected as key events plus a final text commit.
- Gyro/accelerometer: not in the first milestone (listed in section 8).

### 4.6 Audio: `AudioDriverSwitch`

libnx `audout`: 48 kHz, stereo, 16-bit. Three `AudioOutBuffer`s of 480
frames (1920 data bytes in a 4096-byte, 0x1000-aligned block each), about
20 to 30 ms queued ahead of the DSP.

Thread (priority 0x2B via `svcSetThreadPriority`):
```
loop while active:
  if paused: wait on resume event; continue
  lock(); audio_server_process(480, mix_buf); unlock()
  convert int32 → int16 (>> 16) into the free buffer
  audoutAppendAudioOutBuffer(buf)
  audoutWaitPlayFinish(&released, &n, 100 ms timeout)   // never block under lock()
```
Sleep pauses the loop and flushes; resume restarts. If `audoutInitialize`
fails, the driver reports failure and Godot falls back to the dummy driver,
so the game runs silent rather than aborting. `audren` remains documented as
a fallback if `audout` stutters on hardware.

### 4.7 Files and paths

- SD root `sdmc:/`. Game folder `sdmc:/switch/<game>/`.
- `.pck` discovery, in order: (1) `argv[0]` with `.nro` → `.pck`;
  (2) a `.pck` sharing the NRO's basename in the current working directory;
  (3) `game.pck` in the working directory. Emulators do not reliably pass
  `argv`, so steps 2 and 3 exist. Failure shows the full-screen message naming
  the paths tried.
- `user://` → `sdmc:/switch/<game>/user/`; `get_cache_path()` → `.../user/cache/`;
  `get_config_path()` → `.../user/config/`; `get_data_path()` → `.../user/`;
  `get_temp_path()` → `.../user/tmp/`. Created at startup; on failure the
  game runs read-only with a warning.
- `get_executable_path()` returns the NRO path from `argv[0]` or the fallback
  location.
- RomFS is not mounted in the first milestone.

### 4.8 Texture policy

The Switch export preset sets `texture_format/s3tc_bptc = true`,
`etc2_astc = false`. The `switch` feature tag plus the forced
`etc2_supported = false` guarantee no texture is CPU-transcoded at load.

## 5. Build configuration

`platform/switch/detect.py`:

- `can_build()`: `DEVKITPRO` set and `$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc` present.
- Toolchain: `aarch64-none-elf-{gcc,g++,ar,ranlib,strip}`; `arch = arm64`.
- CCFLAGS: `-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -ffunction-sections -fdata-sections -D__SWITCH__`.
- Includes: `$DEVKITPRO/libnx/include`, `$DEVKITPRO/portlibs/switch/include`.
- LINKFLAGS: `-specs=$DEVKITPRO/libnx/switch.specs -fPIE -Wl,--gc-sections`.
- LIBPATH: `$DEVKITPRO/libnx/lib`, `$DEVKITPRO/portlibs/switch/lib`.
- LIBS in order: `EGL GLESv2 glapi drm_nouveau nx m`.
- CPPDEFINES: `SWITCH_ENABLED HORIZON_ENABLED GLES3_ENABLED GLES_API_ENABLED PLATFORM_THREAD_OVERRIDE`.
  Not defined: `UNIX_ENABLED` (it would compile `os_unix.cpp`, the Unix
  sockets and pipes, and the `unix://` debugger peer, none of which link on
  libnx; the two Unix file drivers are guarded with `HORIZON_ENABLED`
  instead), `VULKAN_ENABLED`, `RD_ENABLED`, `GLAD_ENABLED`, `EGL_ENABLED`,
  `CRASH_HANDLER_ENABLED`.
- Forced options: `opengl3=yes vulkan=no use_volk=no use_sowrap=no builtin_*=yes`.
- Disabled modules: `mono openxr webxr camera enet upnp webrtc websocket
  mobile_vr`. Everything bundled in `thirdparty/` builds (jolt, text_server_adv,
  gdscript, etc.).
- Targets: `template_release`, `template_debug`. Output
  `bin/godot.switch.<target>.arm64.elf`; a post-build action runs `nacptool`
  and `elf2nro` to emit `bin/godot.switch.<target>.arm64.nro` with
  `platform/switch/tools/default_icon.jpg`.

Engine guards: `drivers/unix/file_access_unix.cpp` and `dir_access_unix.cpp`
compile under `UNIX_ENABLED || HORIZON_ENABLED`, and the former skips
`<sys/xattr.h>` under `HORIZON_ENABLED`; `core/os/thread.h` already includes `platform_thread.h`
under `PLATFORM_THREAD_OVERRIDE`.

## 6. Packaging, phase 1

`platform/switch/tools/package.py --pck game.pck --name "Title" --author "Me" --version 1.0.0 [--icon icon.jpg] [--elf bin/godot.switch.template_release.arm64.elf] --out ./sdmc`
produces `sdmc/switch/<name>/<name>.nro` (built from the template ELF with
the given NACP and icon via `nacptool` + `elf2nro`; `--elf` defaults to the
release template in `bin/`) and `sdmc/switch/<name>/<name>.pck`. Copy the `switch/` folder to the SD card.
Launch via title override for application mode.

## 7. Debug workflow

- stdout/stderr: under `nxlink`, `nxlinkStdio()` is called and
  `setvbuf(stdout, NULL, _IOLBF, 0)` forces line buffering.
- Always: `SwitchLogger` mirrors every line to `svcOutputDebugString`
  (visible in emulator logs) and, when `user://` is writable, Godot's
  rotating file logger writes `user://logs/godot.log` (project setting
  `debug/file_logging/enable_file_logging` forced on for the Switch build).
- Crashes: Atmosphère fatal reports on SD. The template ELF keeps symbols
  (`debug_symbols=yes`); the NRO carries none. `aarch64-none-elf-addr2line`
  against the ELF resolves report addresses. No in-process backtrace. GDB stub deferred.
- Emulator: Eden on the Windows host, launched with `-p "<arg>"` so libnx
  gets `argv`.

## 8. Testing and phases

Tiers: desktop Compatibility run of the game with the Switch texture policy
and 3D settings; Eden (application mode); hardware via `nxlink` with title
takeover.

| Phase | Deliverable | Pass condition |
|---|---|---|
| 0 Proof | Template builds; stripped copy of the heaviest 3D scene boots on hardware | Correct rendering, logs over nxlink, `discard` verified. Escape hatch to NVK if this fails. |
| 1 Platform complete | Input, rumble, touch, swkbd, audio, saves, dock, sleep | Full game plays through on hardware from an SD folder |
| 2 Performance | Warm-up scene; project settings tuned | 720p30 in the heaviest scene; no frame over 50 ms after warm-up |
| 3 Packaging | `EditorExportPlatform`, romfs pck, forwarder NSP | One-click export; home-menu launch in application mode |

Deferred items (explicitly out of the first milestones): networking
(`socketInitialize` + `NetSocket`), gyro/accelerometer, system language
beyond locale, GDB stub, CI build job on `devkitpro/devkita64`, NVK/Mobile
second renderer path, Mesa shader-cache workarounds.

## 9. Risks

| Risk | Mitigation |
|---|---|
| Godot 4 GLES3 has never run on mesa 20.1 nouveau | Phase 0 is first; escape hatch to NVK |
| Every boot recompiles all shaders; first-use hitches | Warm-up scene; low variant count; loading screen |
| `discard` crash from the 3.x port | Tested in phase 0; avoid alpha scissor if real |
| GL resources may not survive sleep | Fallback to full context recreate |
| CPU-bound on three A57 cores (GDScript, Jolt) | Profile in phase 2; this is game-side tuning |
| `audout` stutter on device change | Timeout on every wait; `audren` fallback documented |

## 10. Out of scope

Official NintendoSDK; editor on the console; deko3d backend; upstreaming;
C#/.NET; GDExtension; multiplayer and all networking in the first
milestones; multiple local players.
