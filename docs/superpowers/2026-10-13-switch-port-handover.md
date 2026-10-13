# Switch port handover (2026-10-13)

Read this first on a new machine. It says where the work stands, how to
rebuild it, how to run it, and what is still unverified.

## 1. What exists

Branch `switch-4.7` (fork of Godot `4.7.2-stable`, tag commit `ed1daf0bf0`)
carries a `platform/switch` export template for Nintendo Switch homebrew
(devkitPro devkitA64 + libnx), using the Compatibility (GLES3) renderer over
the `switch-mesa` 20.1 portlib. Vulkan/NVK and deko3d were researched and
set aside; see the spec.

Documents, all committed:

| What | Path |
|---|---|
| Design spec (binding) | `docs/superpowers/specs/2026-10-13-switch-port-design.md` |
| Research behind the GPU decision | `docs/superpowers/specs/2026-10-13-switch-port-research/` |
| Implementation plan (13 tasks) | `docs/superpowers/plans/2026-10-13-switch-port-phase-0-1.md` |
| Execution ledger (every ruling, task status) | `docs/superpowers/plans/2026-10-13-switch-port-phase-0-1-ledger.md` (copy; see §6) |
| Packaging usage | `platform/switch/tools/README.md` |
| Proof scene used for bring-up | `platform/switch/tests/project/` |

Status of the plan:

- Tasks 1-5 and 7-12: implemented, build-verified, committed.
- Task 6 (phase-0 boot on Eden and on hardware): the template builds and
  links; on Eden it now gets past its own pck check and into Godot's
  startup. Rendering on Eden and on hardware is **not yet confirmed**.
- Hardware steps of Tasks 7-11 (applet guard, dock/sleep, input, audio,
  saves): **not yet run**.
- Task 13 (the real game on hardware): not started.
- A fresh-context review was done; its two Critical and two Important
  findings are fixed. Ten minors are deferred (listed in the ledger).

## 2. Set up a new machine

Linux (WSL2 is fine for building; it cannot run the emulator).

1. devkitPro pacman (https://devkitpro.org/wiki/devkitPro_pacman), then:
   `sudo dkp-pacman -S switch-dev switch-mesa switch-glad switch-libdrm_nouveau`
   (`switch-dev` brings devkitA64, libnx, switch-tools: elf2nro, nacptool, nxlink).
   Versions this work was done with: libnx 4.12.0, switch-mesa 20.1.0-5,
   switch-glad 0.1.27, switch-libdrm_nouveau 1.0.1, switch-tools 1.13.1.
2. `export DEVKITPRO=/opt/devkitpro` and put `$DEVKITPRO/tools/bin` on `PATH`
   (the build also needs `DEVKITPRO` in the environment at link time).
3. Python 3 with `scons` (4.x) and `pytest`.
4. For exporting pcks and running host unit tests, a Linux editor build of
   this branch (step 3 below).
5. Eden emulator on a Windows (or native Linux) machine with a Vulkan 1.1 or
   OpenGL 4.6 GPU. Use a **nightly** from https://git.eden-emu.dev/eden-ci/nightly/releases:
   argument passing to NROs was added after the v0.2.1 stable.

## 3. Build and test commands

```
# Switch export templates (debug first; ~6 min cold on 14 cores)
scons platform=switch target=template_debug -j14
scons platform=switch target=template_release -j14
#   -> bin/godot.switch.<target>.arm64.elf and .nro (nacptool + elf2nro run as a post-action)

# Linux editor with unit tests (needed for exports and the host tests)
scons platform=linuxbsd target=editor tests=yes dev_build=yes -j14
bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case="*SwitchHelpers*"
#   expected: 10 test cases, all passing

# Build-script tests
python -m pytest tests/python_build/test_switch_detect.py tests/python_build/test_switch_package.py -q
#   expected: 5 passed

# Export the proof scene and package it
bin/godot.linuxbsd.editor.dev.x86_64 --headless --path platform/switch/tests/project \
    --export-pack Linux /tmp/godot.pck
python3 platform/switch/tools/package.py --pck /tmp/godot.pck --name godot \
    --author "Godot Switch port" --version 0.1.0 \
    --elf bin/godot.switch.template_debug.arm64.elf --out bin/sdmc
#   -> bin/sdmc/switch/godot/godot.nro + godot.pck
```

## 4. Run it

**Eden (Windows).** Copy `bin/sdmc/switch/godot/` to `%APPDATA%\eden\sdmc\switch\godot\`
(or the `user\sdmc` folder of a portable install). Load `godot.nro` with the
argument string `--verbose --main-pack sdmc:/switch/godot/godot.pck`
(Configure > Debug, homebrew arguments; or `eden.exe --program "<string>" <nro>`).
Eden passes `argv[0]` as the word `homebrew` and reports the working
directory as `/`; the template handles both. Log lines from the template
appear as `Debug.Emulated` entries in `%APPDATA%\eden\log\eden_log.txt`.

**Hardware (Atmosphère).** Copy the same folder to the SD card. Launch via
title override (hold R while opening any installed title) so the game gets
the application memory pool; a plain hbmenu/album launch shows a message
and exits by design. For iteration: `nxlink -s bin/sdmc/switch/godot/godot.nro`
with hbmenu's netloader open; stdout comes back over the socket.

Expected on screen: a rotating lit box with a shadow, an orange disc with a
hard alpha-scissor edge, and a label with FPS, window size and a boot
counter. Switch A rumbles, B toggles a 440 Hz tone, X opens the system
keyboard into the text box, dock/undock changes the window size, the boot
counter persists in `sdmc:/switch/godot/user/save.json`.

## 5. Where the last session stopped

Last Eden run (after commit `231245de7e`) reached Godot's `Main::setup`;
the remaining log errors at that point were fixed by that commit
(`--main-pack` rejected, IP singleton missing). The next run should show
`EGL 1.4 initialized.` followed by Godot's renderer banner, or an EGL error
code. That output is the next thing to look at.

Known risks still open (spec §9): nobody has run Godot 4's GLES3 path on
`switch-mesa` 20.1 before; shaders recompile on every boot; `discard` once
crashed a 3.x port; GL resources may not survive sleep.

## 6. Resuming with the Superpowers skills

The executing-plans skill keeps its ledger at
`.superpowers/sdd/2026-10-13-switch-port-phase-0-1/progress.md`, which is
git-ignored. On a new machine recreate it from the committed copy:

```
mkdir -p .superpowers/sdd/2026-10-13-switch-port-phase-0-1
cp docs/superpowers/plans/2026-10-13-switch-port-phase-0-1-ledger.md \
   .superpowers/sdd/2026-10-13-switch-port-phase-0-1/progress.md
```

Then resume at the first task without a `complete` line (Task 6), and keep
both copies in step when you add rulings.

## 7. Engine and third-party changes to carry on a 4.7.x rebase

- `drivers/unix/{file,dir}_access_unix.{h,cpp}`: compile under `HORIZON_ENABLED`; no xattr.
- `drivers/gles3/storage/{render_scene_buffers_gles3,texture_storage}.cpp`: multiview / multisampled-3D guards.
- `drivers/gles3/storage/config.cpp`: `etc2_supported = false` on Horizon.
- `main/main.cpp`: `enable_file_logging.switch` default.
- `modules/jolt_physics/SCsub`: `JPH_PLATFORM_BSD` on switch.
- `modules/mbedtls/SCsub` (+ `mbedtls_ms_time_switch.cpp`), `thirdparty/mbedtls/library/common.h`,
  `thirdparty/mbedtls/include/godot_module_mbedtls_config.h`.
- `thirdparty/manifold/src/svd.h`: `#undef _S`.
- Disabled on switch: raycast (embree), PCRE2 JIT, mono, openxr, webxr, camera, networking modules.
