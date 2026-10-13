# Install the devkitPro Switch portlibs

Type: task
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

`/opt/devkitpro/portlibs/switch` currently holds only `bin/` (`pkg-config`, `cmake`); the GPU stack is absent. Already installed: `switch-tools 1.13.1-1` (`elf2nro`, `nacptool`), `switch-examples 20260201-1`, `switch-cmake`, `switch-pkg-config`. Install via `dkp-pacman` the GPU stack named by the GPU-stack research: `switch-mesa 20.1.0-5`, `switch-glad 0.1.27-1`, `switch-libdrm_nouveau 1.0.1-2` (or the `switch-dev` meta package), so the build-configuration decision and the hello-triangle prototype can link. HITL if `sudo` is needed: hand the human a checklist. Also install SCons 4.0+ for the host Python (`pip install --user scons`), which is absent in this WSL2 environment. Record installed package versions and the paths later tickets link against. If the GPU-stack research names extra packages, add them here.

## Comments

**2026-10-13, agent (partial, HITL hand-off).** Facts established:
- This WSL2 distro is Arch-based; devkitPro's `dkp-libs`/`dkp-linux` repos are in `/etc/pacman.conf`, so the system `pacman` (not `dkp-pacman`) installs Switch packages. `sudo` needs a password, so the agent cannot run it.
- Already installed: `devkitA64 r30-1`, `devkitA64-gdb 14.1-1`, `libnx 4.12.0-1`, `switch-tools 1.13.1-1`, `switch-examples 20260201-1`, `switch-cmake`, `switch-pkg-config`.
- Available in `dkp-libs`: `switch-mesa 20.1.0-5` (depends on `switch-libdrm_nouveau`), `switch-glad 0.1.27-1` (depends on `switch-mesa`), `switch-libdrm_nouveau 1.0.1-2`.
- SCons: installed `scons 4.11.1` as a user tool via `uv tool install scons` (`~/.local/bin/scons`). Verified on `switch-4.7`: `scons platform=zzz` lists `linuxbsd` as the only buildable platform, confirming discovery works and that no `platform/switch` exists yet.

Human checklist (one command, then verification):

```
sudo pacman -S switch-mesa switch-glad switch-libdrm_nouveau
pacman -Q switch-mesa switch-glad switch-libdrm_nouveau
ls /opt/devkitpro/portlibs/switch/lib/libEGL.a /opt/devkitpro/portlibs/switch/lib/libglapi.a /opt/devkitpro/portlibs/switch/lib/libGLESv2.a /opt/devkitpro/portlibs/switch/lib/libdrm_nouveau.a
/opt/devkitpro/portlibs/switch/bin/aarch64-none-elf-pkg-config --libs egl glesv2
```

Resolve this ticket (next `/wayfinder` session) once the four `.a` files exist; record the `pkg-config --libs` output in the Answer.

## Answer

Done on 2026-10-13 (human ran the `sudo pacman` step; agent did the rest and verified).

Installed packages (system `pacman`, devkitPro `dkp-libs` repo): `switch-mesa 20.1.0-5`, `switch-glad 0.1.27-1`, `switch-libdrm_nouveau 1.0.1-2`, on top of the pre-existing `devkitA64 r30-1`, `devkitA64-gdb 14.1-1`, `libnx 4.12.0-1`, `switch-tools 1.13.1-1` (`elf2nro`, `nacptool`), `switch-examples 20260201-1`, `switch-cmake`, `switch-pkg-config`. Host SCons: `scons 4.11.1` at `~/.local/bin/scons` (installed via `uv tool install scons`; make sure `~/.local/bin` is on `PATH` for build sessions).

Paths later tickets link against:
- Libraries in `/opt/devkitpro/portlibs/switch/lib/`: `libEGL.a` (22 MB, whole mesa driver), `libglapi.a`, `libGLESv2.a`, `libGLESv1_CM.a`, `libdrm_nouveau.a` (+ `libdrm_nouveaud.a` debug), `libglad.a`.
- Headers in `/opt/devkitpro/portlibs/switch/include/`: `EGL/`, `GL/`, `GLES/`, `GLES2/`, `GLES3/`, `KHR/`, `glad/glad.h`, `nouveau.h`, `nouveau_drm.h`, `nvif/`.
- `pkg-config` (`/opt/devkitpro/portlibs/switch/bin/aarch64-none-elf-pkg-config`):
  - `--cflags egl glesv2` → `-I/opt/devkitpro/portlibs/switch/include -DEGL_NO_X11`
  - `--libs egl glesv2` → `-L/opt/devkitpro/portlibs/switch/lib -lEGL -ldrm_nouveau -lGLESv2 -lm -lglapi -lm`
  - `egl.pc` reports `Version: 20.1.0-rc3`.
- Note for the build-configuration ticket: `-lnx` is not in the pkg-config output and must be appended by the platform (`-specs=/opt/devkitpro/libnx/switch.specs -lnx`), and `libglad.a` is devkitPro's glad, distinct from Godot's `thirdparty/glad`; the GPU-stack research recommends using Godot's glad against `eglGetProcAddress` rather than linking `-lglad`.
