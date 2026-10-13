# Platform layer shape for the first milestone

Type: grilling
Status: resolved
Blocked by: 01, 03, 04
Map: ../map.md

## Question

Which Godot interfaces does `platform/switch` implement for the first milestone, and how are they split: `OS_Switch` (main loop around `appletMainLoop`, `user://`/`res://` roots, time, env, process-less), `DisplayServerSwitch` (single window, EGL context), `AudioDriverSwitch`, `JoypadSwitch`/touch feeding `Input`, plus `platform_config.h` and `godot_switch.cpp` entry. What does the file layout look like, mirroring `platform/linuxbsd` vs `platform/web`? Which `OS` methods are stubbed vs implemented?

Context: the prior-art ticket recommends starting from Ozuba/godot `switch-platform` (see [research/03-prior-art.md](../research/03-prior-art.md)); decide what to keep, what to replace, and what to add from Stary2001 (applet-mode guard).
Context: the libnx-runtime ticket lists the exact shims (`platform_thread.h` stacks, clock, cpu count, missing process calls, no dlopen, crash handler off); see [research/04-libnx-runtime.md](../research/04-libnx-runtime.md).

## Answer

Decided with the human on 2026-10-13.

**Shape**: `OS_Switch : OS` directly (not `OS_Unix`), one `DisplayServerSwitch : DisplayServer`, `JoypadSwitch` polled from the display server, `AudioDriverSwitch` (audout), a thread override, and a `main()` that spawns the engine on its own 8 MiB thread. Ozuba/godot `switch-platform` is the skeleton to lift, with `HORIZON_ENABLED` renamed to `SWITCH_ENABLED` and its Vulkan/NVK files left out.

**File layout** (`platform/switch/`):
- `detect.py`, `SCsub`, `platform_config.h`, `platform_egl.h`, `platform_gl.h`: contents decided by the build-configuration ticket.
- `godot_switch.cpp`: `main()`. Order: stdio setup (`setvbuf`, nxlink when launched that way), applet-mode check (warn loudly and continue; see policy), create an 8 MiB pthread that runs `Main::setup` → `Main::start` → `OS::run` → `Main::cleanup`, join it, exit. Prototype ticket verifies EGL/applet/HID calls work from a non-main thread.
- `os_switch.{h,cpp}`: `OS_Switch : OS`. Copies the applicable `OS_Unix::initialize_core` lines (register `FileAccessUnix`, `DirAccessUnix`, later `IPUnix`/`NetSocketUnix`).
- `platform_thread.{h,cpp}`: `PLATFORM_THREAD_OVERRIDE`; honours `Thread::Settings.stack_size` when set, defaults to 1 MiB (libnx default is 128 KiB).
- `display_server_switch.{h,cpp}`: single window on `nwindowGetDefault()`, touch → `InputEventScreenTouch/Drag`, `virtual_keyboard_show` via `swkbd`, operation-mode changes via `appletHook`, owns and polls `JoypadSwitch` in `process_events()`. Mouse cursor, multi-window, IME, clipboard, screensaver: unsupported.
- `gl_manager_switch.{h,cpp}`: a Switch-specific EGL/GL manager (Godot's generic `EGLManager` cannot link against mesa 20.1; see the GPU-stack ticket).
- `joypad_switch.{h,cpp}`: libnx HID (mapping decided by the input ticket).
- `audio_driver_switch.{h,cpp}`: audout (design decided by the audio-driver ticket).
- `switch_logger.{h,cpp}`, `crash_handler_switch.{h,cpp}` (stub, `CRASH_HANDLER_ENABLED` off): sinks decided by the debug-workflow ticket.
- `export/` and `tools/`: later phases (export preset, packaging script).

**Guards outside the platform dir** (`SWITCH_ENABLED`): `drivers/unix/{file_access_unix,dir_access_unix,ip_unix,net_socket_unix,thread_posix}`, `drivers/gles3/{rasterizer_gles3,render_scene_buffers_gles3,storage/texture_storage}`, `thirdparty` touch-ups per the prior-art findings (`enet_godot.h`, `mbedtls timing.c`, `manifold svd.h`, `basisu_enc.cpp`, Jolt `Core.h`), `thirdparty/glad/gl.c` `dlfcn.h` stub. `core/os/os_unix.cpp` is untouched.

**`OS` methods: implemented** — `initialize`/`finalize`, `get_name` → `"Switch"`, `get_distribution_name` → `"Horizon"`, `get_version` via `hosversionGet`, environment via `getenv`/`setenv`, `get_datetime`/`get_time_zone_info` via newlib, `delay_usec` via `svcSleepThread`, `get_ticks_usec` via `armGetSystemTick`, `get_entropy` via `randomGet`, `get_processor_count` → 3, `get_video_adapter_driver_info` → nouveau/mesa 20.1, `has_feature` → `switch`, `horizon`, `arm64`, plus `mobile`-class tags as the input/display tickets decide, `get_user_data_dir`/config/cache/data roots and `get_executable_path` as the paths ticket decides, `create_process` → `envSetNextLoad` chain-load (returns only on failure). **Stubbed** — `execute`, `kill`, `is_process_running`, `get_process_exit_code` (`ERR_UNAVAILABLE`), `get_stdin_string`/`get_stdin_buffer` (empty), `open_dynamic_library` (base `ERR_UNAVAILABLE`), `shell_open`, `alert` → logger.

**Applet-mode policy**: the first milestone continues in applet mode with a loud warning that names the title-override launch; no applet-specific memory tuning. Standing preference recorded on the map: shipped games run in application mode, so application mode is the design target and applet mode is a development convenience.

**Naming**: `platform/switch`, `SWITCH_ENABLED`, `__SWITCH__` kept for libnx, feature tags `switch` and `horizon`.
