# Switch Port, Phases 0 and 1: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A `platform/switch` export template that builds to an NRO, boots a 3D GLES3 scene on Eden and on hardware (phase 0), then supports input, rumble, touch, on-screen keyboard, audio, SD saves, docking and sleep (phase 1), plus the phase-1 packaging script.

**Architecture:** Native SCons platform driving devkitA64 + libnx. `OS_Switch : OS` (not `OS_Unix`) reusing the Unix file/dir drivers; `DisplayServerSwitch` owning a 120-line `GLManagerSwitch` (EGL on the default NWindow, ES 3.1) and `JoypadSwitch`; `AudioDriverSwitch` on libnx `audout`; engine on an 8 MiB thread with 2 MiB worker stacks. Pure logic (paths, sizes, sample conversion, stick/touch maths) lives in a libnx-free header so it is unit-tested on the host; libnx-touching code is verified by cross-compiling and by running on the emulator and console.

**Tech Stack:** Godot 4.7.2 (C++17, SCons 4.11, doctest), devkitA64 (aarch64-none-elf-gcc), libnx 4.12, switch-mesa 20.1.0-5 + switch-glad + switch-libdrm_nouveau portlibs, switch-tools (elf2nro, nacptool, nxlink), Python 3 + pytest for build-script tests, Eden emulator on the Windows host, Atmosphère on hardware.

**Spec:** `docs/superpowers/specs/2026-10-13-switch-port-design.md` (sections 4 to 8 are implemented here; phase 2 tuning and phase 3 packaging are later plans).

## Global Constraints

- Branch `switch-4.7`; source is Godot `4.7.2-stable`. Nothing targets upstream.
- Toolchain from `DEVKITPRO` (`/opt/devkitpro`): `devkitA64/bin/aarch64-none-elf-*`, `libnx`, `portlibs/switch`, `tools/bin/{elf2nro,nacptool,nxlink}`.
- Compiler flags: `-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -ffunction-sections -fdata-sections -D__SWITCH__`. Link: `-specs=$DEVKITPRO/libnx/switch.specs -fPIE -Wl,--gc-sections`. Libs in order: `EGL GLESv2 glapi drm_nouveau nx m`.
- Defines: `SWITCH_ENABLED HORIZON_ENABLED GLES3_ENABLED GLES_API_ENABLED PLATFORM_THREAD_OVERRIDE`. **Amendment to spec §5:** `UNIX_ENABLED` is NOT defined (it would compile `os_unix.cpp`, `net_socket_unix.cpp`, `file_access_unix_pipe.cpp` and the `unix://` debugger peer, none of which link on libnx). `file_access_unix.cpp` and `dir_access_unix.cpp` are guarded with `|| defined(HORIZON_ENABLED)` instead.
- Not defined: `VULKAN_ENABLED RD_ENABLED GLAD_ENABLED EGL_ENABLED CRASH_HANDLER_ENABLED UNIX_ENABLED`.
- Forced build options: `opengl3=yes vulkan=no use_volk=no use_sowrap=no`, `arch=arm64`, targets `template_release`/`template_debug` only. Disabled modules: `mono openxr webxr camera enet upnp webrtc websocket mobile_vr`.
- Output names: `bin/godot.switch.<target>.arm64.elf` and `bin/godot.switch.<target>.arm64.nro`.
- Window sizes: docked `1920x1080`, handheld `1280x720`, from `appletGetOperationMode()`. Surface size never read from `eglQuerySurface`.
- Audio: `audout`, 48000 Hz, 2 channels, int16, 3 buffers of 480 frames, each buffer block 4096 bytes aligned to 0x1000, wait timeout 100 ms.
- Paths: game folder `sdmc:/switch/<game>/`; `user://` = `<game folder>/user/`; cache `user/cache/`; config `user/config/`; temp `user/tmp/`; logs `user/logs/godot.log`. Fallback executable path when `argv[0]` is empty: `<cwd>/game.nro`.
- Applet-mode launch is a hard stop with the message `Launch this game via title override (hold R while opening a title).`
- Thread stacks: engine thread 8 MiB, every other Godot thread 2 MiB.
- Texture formats: S3TC + BPTC only; `etc2_supported = false` on Switch.
- Host unit tests live in `tests/platform/switch/` and run with the Linux editor built with `tests=yes`.
- Every commit message ends with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.

## Review Focus

1. `argv[0]` empty (emulator launched without `-p`): the template must still find `<cwd>/game.pck` and not crash in path code. Pinned in Task 4.
2. A `.pck` whose basename differs from the NRO's (user renamed one file): the missing-pck screen must name every path tried, not a blank screen. Pinned in Task 7.
3. Dock/undock while a frame is in flight: resize must happen on the engine thread, never inside the applet hook. Pinned in Task 8 (hook only sets flags; test asserts the flag path).
4. Audio device change or sleep mid-buffer: `audoutWaitPlayFinish` must time out rather than block while the mixer lock is held. Pinned in Task 10.
5. Touch at the window edge in docked mode: coordinates scale from the 1280x720 touch space to 1920x1080 without overflow. Pinned in Task 4.

---

### Task 1: SCons platform skeleton

**Files:**
- Create: `platform/switch/detect.py`, `platform/switch/SCsub`, `platform/switch/platform_config.h`, `platform/switch/platform_gl.h`, `platform/switch/platform_egl.h`, `platform/switch/godot_switch.cpp` (stub `main`), `platform/switch/logo.svg` (copy of `platform/linuxbsd/logo.svg`), `platform/switch/run_icon.svg` (same)
- Test: `tests/python_build/test_switch_detect.py`

**Interfaces:**
- Produces: `detect.py` functions `get_name() -> "Switch"`, `can_build() -> bool`, `get_opts() -> list`, `get_flags() -> dict`, `configure(env)`; `devkitpro_root(environ: dict) -> str | None` (pure helper used by `can_build` and tests).
- Produces: defines listed in Global Constraints; `platform_config.h` with `#define PLATFORM_THREAD_OVERRIDE` and `#include <alloca.h>`; `platform_gl.h` as Android's (`GLES_API_ENABLED`, `<GLES3/gl32.h>`, `<GLES3/gl3ext.h>`); `platform_egl.h` with `<EGL/egl.h>`, `<EGL/eglext.h>`.

- [ ] **Step 1: Write the failing test** `tests/python_build/test_switch_detect.py`

```python
import importlib.util, os, sys
def _load():
    spec = importlib.util.spec_from_file_location("switch_detect", "platform/switch/detect.py")
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

def test_devkitpro_root_requires_compiler(tmp_path):
    m = _load()
    assert m.devkitpro_root({}) is None
    assert m.devkitpro_root({"DEVKITPRO": str(tmp_path)}) is None
    gcc = tmp_path / "devkitA64" / "bin" / "aarch64-none-elf-gcc"
    gcc.parent.mkdir(parents=True); gcc.write_text("")
    assert m.devkitpro_root({"DEVKITPRO": str(tmp_path)}) == str(tmp_path)

def test_flags_force_gles_and_arm64():
    f = _load().get_flags()
    assert f["arch"] == "arm64" and f["opengl3"] is True and f["vulkan"] is False
    assert f["use_volk"] is False and f["use_sowrap"] is False
    for mod in ["mono", "openxr", "webxr", "camera", "enet", "upnp", "webrtc", "websocket", "mobile_vr"]:
        assert f[f"module_{mod}_enabled"] is False
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -I -m pytest tests/python_build/test_switch_detect.py -q`
Expected: FAIL (file `platform/switch/detect.py` not found).

- [ ] **Step 3: Implement `platform/switch/detect.py`**

Follow `platform/web/detect.py` for shape. `get_flags()` returns the dict asserted above plus `"target": "template_release"`, `"supported_arches"`-free (validate with `validate_arch(env["arch"], get_name(), ["arm64"])` inside `configure`). `configure(env)`: set `CC/CXX/AR/RANLIB/STRIP` from `devkitpro_root(os.environ)`; append the Global Constraints CCFLAGS/LINKFLAGS/CPPDEFINES/LIBPATH/LIBS/CPPPATH (`$DEVKITPRO/libnx/include`, `$DEVKITPRO/portlibs/switch/include`); `env["PROGSUFFIX"] = ".elf"`; error out with `print_error` if `target == "editor"`. Add a post-action on the program node that runs `nacptool --create Godot "Godot Engine" <version> bin/godot.nacp` then `elf2nro <elf> <nro> --icon=platform/switch/tools/default_icon.jpg --nacp=bin/godot.nacp` (icon added in Task 12; until then omit `--icon` when the file is absent). `can_build()` returns `devkitpro_root(os.environ) is not None`.

`platform/switch/SCsub`: `env.add_source_files(env.platform_sources, "*.cpp")` then `env.add_program("#bin/godot", ...)` the way `platform/linuxbsd/SCsub` does.

`godot_switch.cpp` for now: `int main(int argc, char **argv) { return 0; }`.

- [ ] **Step 4: Run tests and the SCons dry run**

Run: `python -I -m pytest tests/python_build/test_switch_detect.py -q`
Expected: 2 passed.
Run: `scons platform=switch target=template_debug -n 2>&1 | grep -m1 aarch64-none-elf-g++`
Expected: one compile line containing `-mtune=cortex-a57` and `-DHORIZON_ENABLED`.

- [ ] **Step 5: Commit** `git add platform/switch tests/python_build/test_switch_detect.py && git commit -m "Add Switch SCons platform skeleton"`

### Task 2: Engine-side guards

**Files:**
- Modify: `drivers/unix/file_access_unix.cpp:33` and `:41`, `drivers/unix/dir_access_unix.cpp:33`
- Modify: `drivers/gles3/storage/render_scene_buffers_gles3.cpp:65,249,320,496,524`, `drivers/gles3/storage/texture_storage.cpp:2613,2650`
- Modify: `drivers/gles3/storage/config.cpp:104`

**Interfaces:** Produces compile-clean `FileAccessUnix`, `DirAccessUnix`, and `drivers/gles3` objects for the Switch toolchain.

- [ ] **Step 1: Verify the failures first**

Run: `scons platform=switch target=template_debug drivers/unix/file_access_unix.switch.template_debug.arm64.o drivers/gles3/storage/render_scene_buffers_gles3.switch.template_debug.arm64.o`
Expected: FAIL. `file_access_unix` produces an empty object (guard false) or `sys/xattr.h: No such file`; `render_scene_buffers_gles3` fails on `glFramebufferTextureMultiviewOVR` / `glTexImage3DMultisample` undeclared.

- [ ] **Step 2: Apply the guards**

`file_access_unix.cpp` and `dir_access_unix.cpp`: `#if defined(UNIX_ENABLED) || defined(HORIZON_ENABLED)`. `file_access_unix.cpp:41`: wrap `#include <sys/xattr.h>` in `#ifndef HORIZON_ENABLED`, and in the two `getxattr` call sites return `ERR_UNAVAILABLE` under `HORIZON_ENABLED`. gles3: change every `#ifndef IOS_ENABLED` and `#if !defined(IOS_ENABLED) && !defined(WEB_ENABLED)` at the listed lines to also exclude `HORIZON_ENABLED`. `config.cpp:104`: after `etc2_supported = true;` add `#ifdef HORIZON_ENABLED etc2_supported = false; #endif`.

- [ ] **Step 3: Verify the objects now build**

Run the Step 1 command plus `drivers/unix/dir_access_unix.switch.template_debug.arm64.o drivers/gles3/storage/texture_storage.switch.template_debug.arm64.o drivers/gles3/storage/config.switch.template_debug.arm64.o`
Expected: all five objects built, no errors.

- [ ] **Step 4: Verify the Linux build still compiles these files**

Run: `scons platform=linuxbsd target=editor drivers/unix/file_access_unix.linuxbsd.editor.x86_64.o drivers/gles3/storage/render_scene_buffers_gles3.linuxbsd.editor.x86_64.o`
Expected: both built.

- [ ] **Step 5: Commit** `git commit -am "Guard Unix file drivers and GLES3 multiview paths for Horizon"`

### Task 3: Thread override

**Files:**
- Create: `platform/switch/platform_thread.h`, `platform/switch/platform_thread.cpp`
- Reference: `drivers/apple/thread_apple.h`, `drivers/apple/thread_apple.cpp:93-128`

**Interfaces:** Produces the `Thread` class Godot expects under `PLATFORM_THREAD_OVERRIDE` (same public API as `drivers/apple/thread_apple.h`: `typedef void (*Callback)(void *)`, `typedef uint64_t ID`, `start(Callback, void*, const Settings&) -> ID`, `wait_to_finish()`, `get_caller_id()`, `get_main_id()`, `is_main_thread()`, `make_main_thread()`, `release_main_thread()`, `set_name(const String&) -> Error`, `is_started()`, `yield()`), plus `static constexpr size_t SWITCH_THREAD_STACK_BYTES = 2 * 1024 * 1024;`.

- [ ] **Step 1: Copy `thread_apple.h/.cpp` to `platform_thread.h/.cpp`**, rename nothing public, replace the QoS block with `pthread_attr_setstacksize(&attr, SWITCH_THREAD_STACK_BYTES)` for every priority, map `yield()` to `sched_yield()`, and implement `set_name` as a no-op returning `OK` (libnx has no `pthread_setname_np`).

- [ ] **Step 2: Verify it compiles**

Run: `scons platform=switch target=template_debug platform/switch/platform_thread.switch.template_debug.arm64.o core/os/thread.switch.template_debug.arm64.o`
Expected: both built.

- [ ] **Step 3: Commit** `git add platform/switch/platform_thread.* && git commit -m "Add Switch thread override with 2 MiB stacks"`

### Task 4: Host-testable helpers

**Files:**
- Create: `platform/switch/switch_helpers.h` (header-only, includes only `core/` headers, never `<switch.h>`)
- Test: `tests/platform/switch/test_switch_helpers.cpp`

**Interfaces (namespace `SwitchHelpers`):**
- `Size2i window_size_for_docked(bool p_docked)` → `(1920,1080)` / `(1280,720)`.
- `String fallback_executable_path(const String &p_cwd)` → `p_cwd.path_join("game.nro")`.
- `String executable_path(const String &p_argv0, const String &p_cwd)` → `p_argv0` if non-empty, else the fallback.
- `String user_dir_for_executable(const String &p_exec_path)` → `exec_dir/user`.
- `Vector2 touch_to_window(int p_x, int p_y, const Size2i &p_window)` → scales from 1280x720 touch space.
- `float stick_axis(int32_t p_raw)` → clamp(`p_raw / 32767.0f`, -1, 1).
- `void mix_to_int16(const int32_t *p_src, int16_t *p_dst, int p_samples)` → `p_dst[i] = p_src[i] >> 16`.
- `Vector<String> pck_candidates(const String &p_exec_path)` → `[exec.pck (ext swap), exec_dir/basename.pck]` for the missing-pck message.

- [ ] **Step 1: Write the failing tests**

```cpp
#include "tests/test_macros.h"
TEST_FORCE_LINK(test_switch_helpers)
#include "platform/switch/switch_helpers.h"
namespace TestSwitchHelpers {
TEST_CASE("[SwitchHelpers] window size follows dock state") {
	CHECK(SwitchHelpers::window_size_for_docked(true) == Size2i(1920, 1080));
	CHECK(SwitchHelpers::window_size_for_docked(false) == Size2i(1280, 720));
}
TEST_CASE("[SwitchHelpers] executable path falls back when argv0 is empty") {
	CHECK(SwitchHelpers::executable_path("", "sdmc:/") == "sdmc:/game.nro");
	CHECK(SwitchHelpers::executable_path("sdmc:/switch/mygame/mygame.nro", "sdmc:/") == "sdmc:/switch/mygame/mygame.nro");
	CHECK(SwitchHelpers::user_dir_for_executable("sdmc:/switch/mygame/mygame.nro") == "sdmc:/switch/mygame/user");
}
TEST_CASE("[SwitchHelpers] pck candidates list both names") {
	Vector<String> c = SwitchHelpers::pck_candidates("sdmc:/switch/mygame/mygame.nro");
	CHECK(c.size() == 2);
	CHECK(c[0] == "sdmc:/switch/mygame/mygame.pck");
	CHECK(c[1] == "sdmc:/switch/mygame/mygame.nro.pck");
}
TEST_CASE("[SwitchHelpers] touch scales to docked window without overflow") {
	CHECK(SwitchHelpers::touch_to_window(1279, 719, Size2i(1920, 1080)).is_equal_approx(Vector2(1918.5f, 1078.5f)));
	CHECK(SwitchHelpers::touch_to_window(0, 0, Size2i(1280, 720)) == Vector2(0, 0));
}
TEST_CASE("[SwitchHelpers] stick and sample conversion") {
	CHECK(SwitchHelpers::stick_axis(32767) == doctest::Approx(1.0f));
	CHECK(SwitchHelpers::stick_axis(-40000) == doctest::Approx(-1.0f));
	int32_t src[2] = { 0x7FFF0000, (int32_t)0x80000000 }; int16_t dst[2];
	SwitchHelpers::mix_to_int16(src, dst, 2);
	CHECK(dst[0] == 32767); CHECK(dst[1] == -32768);
}
}
```

- [ ] **Step 2: Build the host test binary and verify it fails**

Run: `scons platform=linuxbsd target=editor tests=yes dev_build=yes -j$(nproc)` then `bin/godot.linuxbsd.editor.dev.x86_64 --test --test-case="*SwitchHelpers*"`
Expected: compile FAIL (header missing).

- [ ] **Step 3: Implement `switch_helpers.h`** with the signatures above as `static inline` functions.

- [ ] **Step 4: Verify** same command; Expected: `5 test cases passed`.

- [ ] **Step 5: Commit** `git add platform/switch/switch_helpers.h tests/platform/switch && git commit -m "Add host-tested Switch helper functions"`

### Task 5: OS layer and entry point

**Files:**
- Create: `platform/switch/os_switch.h`, `platform/switch/os_switch.cpp`, `platform/switch/switch_logger.h`, `platform/switch/switch_logger.cpp`
- Modify: `platform/switch/godot_switch.cpp`
- Reference: `platform/android/os_android.*` for the override set; `core/os/os.h:132-281` for pure virtuals.

**Interfaces:**
- `class OS_Switch : public OS` with: `initialize_core()` (registers `FileAccessUnix` for `ACCESS_RESOURCES/USERDATA/FILESYSTEM`, `DirAccessUnix` likewise, no pipe, no sockets); `initialize()`, `finalize()`, `finalize_core()`; `set_main_loop/get_main_loop/delete_main_loop`; `get_name() -> "Switch"`, `get_distribution_name() -> "Horizon"`, `get_version()` from `hosversionGet()` as `"X.Y.Z"`; `_check_internal_feature_support`: `switch`, `arm64`, `gles3`, `s3tc`, `bptc`, `mobile` false; `get_processor_count() -> 3`; `get_ticks_usec()` from `clock_gettime(CLOCK_MONOTONIC)`; `delay_usec` via `nanosleep`; `get_datetime/get_time_zone_info` via `localtime_r`; `execute/create_process/kill/is_process_running/get_process_exit_code` return `ERR_UNAVAILABLE` / false / -1; `get_entropy` via `randomGet`; environment via `getenv/setenv`; `get_locale()` via `setGetSystemLanguage`+`setMakeLanguage` mapped to Godot locale strings (`en`, `ja`, `fr`, `de`, `it`, `es`, `zh_CN`, `ko`, `nl`, `pt`, `ru`, `zh_TW`, else `en`); `get_executable_path()` = `SwitchHelpers::executable_path(argv0, cwd)`; `get_user_data_dir()`, `get_data_path()`, `get_config_path()`, `get_cache_path()`, `get_temp_path()` per Global Constraints; `get_video_adapter_driver_info()` returns `["nouveau", "switch-mesa 20.1"]`; `get_stdin_string/get_stdin_buffer` return empty; `alert(const String&, const String&)` logs via `print_error`.
- `void OS_Switch::set_argv0(const char *)` and `String get_argv0() const` used by main.
- `class SwitchLogger : public Logger` with `logv(const char *p_format, va_list p_list, bool p_err)` writing to `stdout` and `svcOutputDebugString`.
- `godot_switch.cpp`: `main(int argc, char **argv)`: `socketInitializeDefault()` + `nxlinkStdio()` + `setvbuf(stdout, nullptr, _IOLBF, 0)` in `DEBUG_ENABLED` builds; construct `OS_Switch`; spawn `engine_thread` (8 MiB via `threadCreate`) running `Main::setup(argv0, argc, argv)`, `Main::start()`, loop `while (appletMainLoop() && !Main::iteration())`, `Main::cleanup()`; join; `socketExit()`; return.

- [ ] **Step 1: Implement the four files** per the interface list. `initialize_core()` also calls `_set_logger(memnew(CompositeLogger({ memnew(SwitchLogger) })))`.

- [ ] **Step 2: Verify compile of the platform objects**

Run: `scons platform=switch target=template_debug platform/switch/os_switch.switch.template_debug.arm64.o platform/switch/godot_switch.switch.template_debug.arm64.o platform/switch/switch_logger.switch.template_debug.arm64.o`
Expected: three objects built.

- [ ] **Step 3: Commit** `git add platform/switch && git commit -m "Add OS_Switch, logger, and engine-thread entry point"`

### Task 6: GL manager, display server, first link, phase-0 boot

**Files:**
- Create: `platform/switch/gl_manager_switch.h/.cpp`, `platform/switch/display_server_switch.h/.cpp`, `platform/switch/tests/project/project.godot` plus `main.tscn` (a rotating `MeshInstance3D` box with a `DirectionalLight3D` casting shadows, a `StandardMaterial3D` quad with `transparency = ALPHA_SCISSOR` to exercise `discard`, a `Label` showing `Engine.get_frames_per_second()`), `platform/switch/tests/project/main.gd`
- Modify: `platform/switch/os_switch.cpp` (`initialize()` calls `DisplayServerSwitch::register_switch_driver()`)
- Reference: `platform/android/display_server_android.h` (override set), research §5 of `docs/superpowers/specs/2026-10-13-switch-port-research/gles3-3d-on-switch-mesa.md` for the EGL sequence.

**Interfaces:**
- `class GLManagerSwitch` : `Error initialize(bool p_docked)`, `void finalize()`, `Error resize(bool p_docked)`, `void make_current()`, `void release_current()`, `void swap_buffers()`, `void set_use_vsync(bool)`, `bool is_using_vsync() const`, `Size2i get_window_size() const`.
- `class DisplayServerSwitch : public DisplayServer` with `static void register_switch_driver()`, `static DisplayServer *create_func(...)` (signature from `servers/display/display_server.h:79`), `static Vector<String> get_rendering_drivers_func() -> ["opengl3"]`; `has_feature()` true for `FEATURE_TOUCHSCREEN`, `FEATURE_VIRTUAL_KEYBOARD`, `FEATURE_SWAP_BUFFERS`; `get_name() -> "Switch"`; `window_get_size()` from the GL manager; `process_events()` calls `appletMainLoop()` and, when false, `Input::parse_input_event` of a `NOTIFICATION_WM_CLOSE_REQUEST` via the main loop; `swap_buffers()`, `window_set_vsync_mode`, `window_get_vsync_mode`; `screen_get_size`, `screen_get_dpi -> 237`; all other pure virtuals stubbed as in Android. Members `void set_resize_pending()`, `void set_focus_pending(bool)`, `void set_resume_pending()` for Task 8.

- [ ] **Step 1: Implement `GLManagerSwitch`** with the sequence: `eglGetDisplay(EGL_DEFAULT_DISPLAY)`, `eglInitialize`, `eglBindAPI(EGL_OPENGL_ES_API)`, `eglChooseConfig` with `EGL_RENDERABLE_TYPE EGL_OPENGL_ES3_BIT, EGL_RED/GREEN/BLUE/ALPHA_SIZE 8, EGL_DEPTH_SIZE 24, EGL_STENCIL_SIZE 8`, `nwindowSetDimensions(nwindowGetDefault(), w, h)`, `eglCreateWindowSurface(display, config, nwindowGetDefault(), nullptr)`, `eglCreateContext` with `EGL_CONTEXT_CLIENT_VERSION 3`, `eglMakeCurrent`, then `RasterizerGLES3::make_current(false)`. In release builds `setenv("MESA_NO_ERROR", "1", 1)` before `eglInitialize`.

- [ ] **Step 2: Implement `DisplayServerSwitch`** per the interface list; the constructor calls `gl_manager.initialize(appletGetOperationMode() == AppletOperationMode_Console)` and fails with `ERR_CANT_CREATE` on error.

- [ ] **Step 3: Full build**

Run: `scons platform=switch target=template_debug -j$(nproc)`
Expected: `bin/godot.switch.template_debug.arm64.elf` and `.nro` exist. Fix undefined references by stubbing in the platform files only; do not touch `drivers/` beyond Task 2.

- [ ] **Step 4: Export the test project pck**

Run: `bin/godot.linuxbsd.editor.dev.x86_64 --headless --path platform/switch/tests/project --export-pack Linux /tmp/claude-1000/-home-aisuko-gamedev-godot-nx/6ae6ab92-2cc8-480b-9a88-357a004877d4/scratchpad/sdmc/switch/godot/godot.pck` (add a Linux export preset to the test project with `texture_format/s3tc_bptc=true`, `etc2_astc=false`).
Then copy `bin/godot.switch.template_debug.arm64.nro` to `.../sdmc/switch/godot/godot.nro`.

- [ ] **Step 5: Human verification on Eden** (application mode): launch `godot.nro` with `-p "sdmc:/switch/godot/godot.nro"`. Expected: the lit box rotates, the alpha-scissor quad shows a hard cut-out, the FPS label updates. Record the emulator log lines for `GL_RENDERER` and any `glGetError`.

- [ ] **Step 6: Human verification on hardware**: `nxlink -s sdmc/switch/godot/godot.nro` with title override held. Expected: same scene; nxlink prints Godot's startup banner with `OpenGL API 3.1 ... nouveau`. If the alpha-scissor quad crashes the GPU, record it: Task 7's project fallback switches it to `ALPHA_HASH` and the spec risk table is updated.

- [ ] **Step 7: Commit** `git add platform/switch && git commit -m "Add Switch GL manager and display server; phase-0 scene boots"`

**Phase 0 gate:** Steps 5 and 6 pass. If step 6 cannot be made to render after fixing platform-layer bugs, stop and report; the spec's NVK escape hatch is a new brainstorm, not a continuation of this plan.

### Task 7: Pre-renderer messages (applet guard, missing pck)

**Files:**
- Create: `platform/switch/switch_message.h/.cpp`
- Modify: `platform/switch/godot_switch.cpp`, `platform/switch/os_switch.cpp`

**Interfaces:**
- `void switch_show_blocking_message(const String &p_text)`: `consoleInit(nullptr)`, print text, `consoleUpdate`, wait for Plus (`HidNpadButton_Plus`), `consoleExit`. Only legal before `GLManagerSwitch::initialize()`.
- In `main` before constructing the engine thread: if `appletGetAppletType()` is neither `AppletType_Application` nor `AppletType_SystemApplication`, show the Global Constraints applet message and `return 0`.
- In `OS_Switch`, override `alert()` to call `switch_show_blocking_message` only while `DisplayServer::get_singleton() == nullptr`, otherwise log.
- Missing pck: in `main`, before `Main::setup`, if none of `SwitchHelpers::pck_candidates(exec_path)` exists (`FileAccess::exists`), show `No game data found. Looked for:\n<candidate 1>\n<candidate 2>` and return.

- [ ] **Step 1: Add a host test** to `tests/platform/switch/test_switch_helpers.cpp`:

```cpp
TEST_CASE("[SwitchHelpers] missing pck message lists every candidate") {
	String msg = SwitchHelpers::missing_pck_message("sdmc:/switch/mygame/mygame.nro");
	CHECK(msg.contains("sdmc:/switch/mygame/mygame.pck"));
	CHECK(msg.contains("sdmc:/switch/mygame/mygame.nro.pck"));
	CHECK(msg.begins_with("No game data found."));
}
```
- [ ] **Step 2: Run host tests; expect compile failure** (`missing_pck_message` undefined).
- [ ] **Step 3: Implement `String SwitchHelpers::missing_pck_message(const String &p_exec_path)`** in `switch_helpers.h` and the three code paths above.
- [ ] **Step 4: Run host tests** → `6 test cases passed`; full Switch build succeeds.
- [ ] **Step 5: Human verification on hardware**: launch from hbmenu without title override → message appears, Plus exits to hbmenu. Rename the pck → missing-data message names both paths.
- [ ] **Step 6: Commit** `git commit -am "Add applet-mode and missing-pck blocking messages"`

### Task 8: Dock, focus, sleep and resume

**Files:**
- Modify: `platform/switch/display_server_switch.h/.cpp`, `platform/switch/gl_manager_switch.cpp`

**Interfaces:**
- `static void DisplayServerSwitch::_applet_hook(AppletHookType p_hook, void *p_param)`: on `AppletHookType_OnOperationMode` → `set_resize_pending()`; on `AppletHookType_OnFocusState` → `set_focus_pending(appletGetFocusState() == AppletFocusState_InFocus)`; on `AppletHookType_OnResume` → `set_resume_pending()`. The hook writes three `SafeFlag`s only.
- `process_events()` consumes the flags on the engine thread: resize → `gl_manager.resize(docked)` then `_window_callback(window_resize_callback, Size2i)` and `NOTIFICATION_WM_SIZE_CHANGED`; focus → `MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN/OUT` via `_send_window_event(WINDOW_EVENT_FOCUS_IN/OUT)` and `AudioDriverSwitch::get_singleton()->set_paused(bool)` (Task 10); resume → `gl_manager.resize(docked)`.
- `GLManagerSwitch::resize`: `eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)`, `eglDestroySurface`, `nwindowSetDimensions`, `eglCreateWindowSurface`, `eglMakeCurrent`; on failure retry once, then `CRASH_NOW_MSG`.

- [ ] **Step 1: Implement**, registering the hook with `appletHook(&hook_cookie, _applet_hook, this)` in the constructor and `appletUnhook` in the destructor. Log each event under `OS::is_stdout_verbose()`.
- [ ] **Step 2: Build** → success.
- [ ] **Step 3: Human verification on hardware**: dock and undock three times while the scene runs → label reports `1920x1080` / `1280x720` via `DisplayServer.window_get_size()`, no GL errors in the log. Close the lid 10 s, reopen → scene continues; log shows FOCUS_OUT then FOCUS_IN. If textures come back black, implement full context recreate in `resize` (destroy context too, `RasterizerGLES3::make_current` again) and note it in the spec risk table.
- [ ] **Step 4: Commit** `git commit -am "Handle dock, focus and resume via applet hook flags"`

### Task 9: Joypad, touch, rumble, on-screen keyboard

**Files:**
- Create: `platform/switch/joypad_switch.h/.cpp`
- Modify: `platform/switch/display_server_switch.cpp` (`process_events` calls `joypad->process()`; `virtual_keyboard_show/hide`), `platform/switch/os_switch.cpp` (`initialize_joypads()`)
- Reference: `platform/linuxbsd/joypad_linux.cpp` for the vibration-timestamp polling pattern; `core/input/input.h:375-458`.

**Interfaces:**
- `class JoypadSwitch` : `JoypadSwitch(Input *p_input)`, `void process()`, `static constexpr int DEVICE_ID = 0`, `static const char *GUID` = `"03000000d620000013a7000000000000"`, name `"Nintendo Switch Controller"`.
- `process()`: `padUpdate(&pad)`; for each of the 16 mapped buttons call `input->joy_button(DEVICE_ID, godot_button, down)` on change; sticks via `padGetStickPos` → `SwitchHelpers::stick_axis` → `joy_axis(LEFT_X, LEFT_Y, RIGHT_X, RIGHT_Y)` (Y negated); ZL/ZR as `TRIGGER_LEFT/RIGHT` 0 or 1. Mapping table: A→`JOY_BUTTON_B`, B→`JOY_BUTTON_A`, X→`JOY_BUTTON_Y`, Y→`JOY_BUTTON_X` (physical positions; Godot's SDL layout is Xbox), L→`LEFT_SHOULDER`, R→`RIGHT_SHOULDER`, Plus→`START`, Minus→`BACK`, StickL/StickR→`LEFT_STICK/RIGHT_STICK`, D-pad→`DPAD_*`.
- Touch: `hidGetTouchScreenStates(&state, 1)`; per finger id, emit `InputEventScreenTouch` (pressed) on new id, `InputEventScreenDrag` with `relative` on moved, `InputEventScreenTouch` (released) on vanished id; positions via `SwitchHelpers::touch_to_window`.
- Rumble: when `input->get_joy_vibration_timestamp(DEVICE_ID)` changes, build `HidVibrationValue{ amp_low = strength.x, freq_low = 160, amp_high = strength.y, freq_high = 320 }` (duration 0 → until next change) and `hidSendVibrationValues(handles, values, 2)`; stop with zeros when `get_joy_vibration_duration` elapsed.
- Keyboard: `DisplayServerSwitch::virtual_keyboard_show(const String &p_existing_text, ...)` runs `swkbdCreate`, `swkbdConfigMakePresetDefault`, `swkbdConfigSetInitialText`, `swkbdShow` synchronously; result text is delivered as `InputEventKey` unicode events after `KEY_BACKSPACE` × existing length; `virtual_keyboard_get_height() -> 0`.

- [ ] **Step 1: Implement** `JoypadSwitch` and the display server hooks; `OS_Switch::initialize_joypads()` constructs it and calls `Input::joy_connection_changed(0, true, name, GUID)`.
- [ ] **Step 2: Extend the test project**: `main.gd` prints every `InputEventJoypadButton`, `InputEventJoypadMotion` (deadzone 0.2), `InputEventScreenTouch`, and `InputEventScreenDrag` to the log and starts rumble (0.5, 1.0, 0.3 s) on A press; a `LineEdit` is focused by pressing X to open the keyboard.
- [ ] **Step 3: Build; human verification on hardware** (handheld, then Pro Controller): every button prints its Godot name; sticks reach ±1.0; ZL/ZR print trigger axes; touch prints coordinates up to `1279,719` in handheld; docked drag prints up to `1918,1078`; rumble felt on A; X opens the keyboard and the typed text lands in the `LineEdit`.
- [ ] **Step 4: Commit** `git add platform/switch && git commit -m "Add Switch joypad, touch, rumble and swkbd input"`

### Task 10: Audio driver

**Files:**
- Create: `platform/switch/audio_driver_switch.h/.cpp`
- Modify: `platform/switch/os_switch.cpp` (`AudioDriverManager::add_driver(&audio_driver)` in the constructor)

**Interfaces:**
- `class AudioDriverSwitch : public AudioDriver` : `get_name() -> "audout"`, `init() -> Error` (`audoutInitialize`, `audoutStartAudioOut`, allocate 3 × `memalign(0x1000, 4096)`, reject if `audoutGetSampleRate() != 48000` or channels != 2 → `ERR_CANT_OPEN`), `start()` spawns the thread via `Thread::start`, `get_mix_rate() -> 48000`, `get_speaker_mode() -> SPEAKER_MODE_STEREO`, `get_latency() -> 0.03f`, `lock/unlock` on a `Mutex`, `finish()` stops the thread, `audoutStopAudioOut`, `audoutExit`, frees buffers; `void set_paused(bool)`; `static AudioDriverSwitch *get_singleton()`.
- Thread body per spec §4.6; `audoutWaitPlayFinish(&released, &count, 100'000'000ULL)`; on timeout loop again without appending. Thread priority raised with `svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B)`.
- On `init()` failure, `AudioDriverManager` falls back to the dummy driver automatically; log a warning.

- [ ] **Step 1: Add a host test** for the conversion step already covered by `mix_to_int16` (Task 4); add one more asserting `SwitchHelpers::audout_buffer_bytes(480) == 1920` and `audout_block_bytes() == 4096`.
- [ ] **Step 2: Run host tests → compile failure; implement the two constexpr helpers; tests pass.**
- [ ] **Step 3: Implement the driver**; wire `set_paused` from Task 8's focus handling.
- [ ] **Step 4: Build; human verification**: test project plays a looping `AudioStreamPlayer` (440 Hz tone) and a one-shot on B press; no crackle in handheld or docked; sleep 10 s then resume → audio continues within 1 s; with Bluetooth headphones paired, disconnect them mid-play → game keeps running (silence or speaker switch, no hang).
- [ ] **Step 5: Commit** `git add platform/switch && git commit -m "Add audout-based AudioDriverSwitch"`

### Task 11: User data, logging, locale on device

**Files:**
- Modify: `platform/switch/os_switch.cpp`, `platform/switch/godot_switch.cpp`

**Interfaces:**
- In `main`, after computing `exec_path`: `DirAccess::make_dir_recursive_absolute` for `user/`, `user/cache`, `user/config`, `user/tmp`, `user/logs`; on failure set `OS_Switch::set_user_dir_writable(false)` and `WARN_PRINT`.
- `OS_Switch::initialize_core()` forces `ProjectSettings` overrides before `Main::setup2`: pass `--log-file user://logs/godot.log` semantics by setting `debug/file_logging/enable_file_logging` default to `true` and `debug/file_logging/log_path` to `user://logs/godot.log` via `GLOBAL_DEF_OVERRIDE`-style defaults in `OS_Switch::initialize()`.
- `get_locale()` as in Task 5, now verified.

- [ ] **Step 1: Implement**.
- [ ] **Step 2: Build; human verification**: test project writes `user://save.json` on Plus and reads it back on next boot (label shows the saved counter); `sdmc:/switch/godot/user/logs/godot.log` contains the boot banner; console language set to Japanese → `OS.get_locale()` printed as `ja`.
- [ ] **Step 3: Commit** `git commit -am "Create user directories, file logging and system locale on Switch"`

### Task 12: Phase-1 packaging script

**Files:**
- Create: `platform/switch/tools/package.py`, `platform/switch/tools/default_icon.jpg` (256x256 JPEG, Godot logo), `platform/switch/tools/README.md` (usage + title-override launch instructions)
- Test: `tests/python_build/test_switch_package.py`
- Modify: `platform/switch/detect.py` (post-action now passes `--icon`)

**Interfaces:**
- `package.py --pck <file> --name <str> --author <str> --version <str> [--icon <jpg>] [--elf <path>] --out <dir>`.
- Pure functions: `plan_commands(args: argparse.Namespace, devkitpro: str) -> list[list[str]]` returning the exact `nacptool` and `elf2nro` argv lists, and `output_layout(out: str, name: str) -> dict` with keys `dir`, `nro`, `pck`, `nacp`. `main()` runs the commands with `subprocess.run(check=True)` and copies the pck.
- Defaults: `--elf` → `bin/godot.switch.template_release.arm64.elf`; `--icon` → `platform/switch/tools/default_icon.jpg`.

- [ ] **Step 1: Write the failing tests**

```python
def test_output_layout():
    m = _load("platform/switch/tools/package.py")
    lay = m.output_layout("/o", "mygame")
    assert lay == {"dir": "/o/switch/mygame", "nro": "/o/switch/mygame/mygame.nro",
                   "pck": "/o/switch/mygame/mygame.pck", "nacp": "/o/switch/mygame/mygame.nacp"}

def test_plan_commands_uses_nacptool_then_elf2nro():
    m = _load("platform/switch/tools/package.py")
    ns = m.parse_args(["--pck", "g.pck", "--name", "mygame", "--author", "Me", "--version", "1.0.0", "--out", "/o"])
    cmds = m.plan_commands(ns, "/opt/devkitpro")
    assert cmds[0] == ["/opt/devkitpro/tools/bin/nacptool", "--create", "mygame", "Me", "1.0.0", "/o/switch/mygame/mygame.nacp"]
    assert cmds[1] == ["/opt/devkitpro/tools/bin/elf2nro", "bin/godot.switch.template_release.arm64.elf",
                       "/o/switch/mygame/mygame.nro", "--icon=platform/switch/tools/default_icon.jpg",
                       "--nacp=/o/switch/mygame/mygame.nacp"]
```

- [ ] **Step 2: Run** `python -I -m pytest tests/python_build/test_switch_package.py -q` → FAIL (module missing).
- [ ] **Step 3: Implement `package.py`** with `parse_args(argv) -> Namespace`, `output_layout`, `plan_commands`, `main()`.
- [ ] **Step 4: Run tests** → 2 passed. Run the script for real against the test project pck and confirm `sdmc/switch/godot/godot.nro` boots on Eden.
- [ ] **Step 5: Commit** `git add platform/switch/tools tests/python_build/test_switch_package.py platform/switch/detect.py && git commit -m "Add phase-1 Switch packaging script"`

### Task 13: Phase-1 acceptance with the real game

**Files:** none new; `docs/superpowers/specs/2026-10-13-switch-port-design.md` §9 risk table updated with hardware findings.

- [ ] **Step 1:** Export the user's game from the Linux editor with the Switch texture policy (`s3tc_bptc` on, `etc2_astc` off), package it with Task 12's script, copy to SD.
- [ ] **Step 2: Human verification on hardware**, application mode: the game plays start to finish; saves persist across reboot; dock/undock and sleep/resume mid-game do not crash; audio and rumble work. Record frame times of the heaviest scene (for the phase-2 plan).
- [ ] **Step 3:** Update the spec risk table with what hardware showed (`discard`, sleep survival, shader-compile hitch duration) and commit: `git commit -am "Record phase-1 hardware findings in the Switch spec"`.

---

## Self-review notes

- Spec coverage: §4.1 → T5, T11; §4.2 → T6, T7, T8; §4.3 → T2, T6; §4.4 → T3, T5; §4.5 → T9; §4.6 → T10; §4.7 → T4, T5, T7, T11; §4.8 → T2 + export preset in T6/T13; §5 → T1, T2; §6 → T12; §7 → T5, T11; §8 phases 0 and 1 → T6 gate, T13. Phase 2 and §8 phase 3 are out of this plan by design.
- Type consistency: `SwitchHelpers` names used in T5, T7, T9, T10 match T4's interface block (`missing_pck_message`, `audout_buffer_bytes`, `audout_block_bytes` are added in T7/T10 and declared there).
- Review Focus items 1 to 5 map to tests in T4 (1, 5), T7 (2), T8 (3, hardware step), T10 (4, hardware step).
