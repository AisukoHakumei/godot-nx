# Prior art: existing Godot ports to Switch homebrew

Type: research
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

Which Godot-on-Switch homebrew ports exist (Stary2001's `godot-switch` for 3.x and any Godot 4 attempts), what engine version each targeted, and how each solved threads, GL context creation, audio, input, file paths, and packaging? Which pieces are liftable into a 4.7 `platform/switch`, and which are dead ends? Cite repositories and files, not blog posts.

## Answer

Two real native ports exist, plus one dead end and one non-port:

- **Ozuba/godot `switch-platform`** (Godot 4.7-rc, Jun-Sep 2026, "heavily vibecoded", builds and boots; GLES3 plus Vulkan/NVK via a community Mesa 26.2 build, not portlibs). **Lift wholesale as the skeleton**: `detect.py`, `platform_thread.*` using Godot's `PLATFORM_THREAD_OVERRIDE` hook (2 MiB pthread stacks; libnx defaults to 128 KiB), `OS_Switch`, `DisplayServerSwitch` EGL path, `JoypadSwitch`, and the `HORIZON_ENABLED` guards in `drivers/unix`, `drivers/gles3`, thirdparty. Drop its Vulkan/NVK and Mesa-26 dependency unless the GPU-stack ticket finds stock `switch-mesa` insufficient. Its README's export addon does not exist in the repo.
- **Stary2001/godot** (3.1.1 to 3.5.2, 2018-2023, shipped games; re-homed as Homebrodot/platform-switch, no 4.x). Take the battle-tested pieces Ozuba lacks: applet-mode guard/splash in `main()`, hbmenu `fileassoc` config + forwarder NRO for ".pck beside NRO" launching, and the C++ exporter that rewrites the NRO `ASET` section (icon/NACP/RomFS) via `nro.h`/`nacp.h`.
- **halotroop2288/godot `4.x-switch`** (4.1-dev, one WIP commit, 2023): never compiled (Godot 3 headers). Dead end.
- **Thorhax/Gizmoduck-NX-Modern**: not a port; loads `libgodot_android.so` behind a fake JNI/Bionic shim.

Common ground across ports: EGL on `nwindowGetDefault()` with `EGL_CONTEXT_CLIENT_VERSION 3`; newlib pthreads via `init_thread_posix()` + `PTHREAD_NO_RENAME`; `user://` mapped to `sdmc:/switch/godot/app_userdata/<name>`; all use `audren` (the audio ticket chose `audout` anyway, on the strength of the libnx 4.10 fix).

Open risk handed to the GPU-stack ticket and the hello-triangle prototype: nobody has run Godot 4.x Compatibility on devkitPro's stock `switch-mesa` (Mesa 20.1.0-rc3, nvc0); Ozuba only ran it through Mesa 26.2.

Findings: [research/03-prior-art.md](../research/03-prior-art.md) (branch `research/prior-art`, commit 23116ed745).
