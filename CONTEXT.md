# Godot Switch Port

A fork of Godot Engine adding a `platform/switch` target that runs exported Godot projects as homebrew on the Nintendo Switch using devkitPro's toolchain.

## Language

**Switch port**:
The `platform/switch` runtime target of this fork, built from Godot `4.7.2-stable` on the `switch-4.7` branch.
_Avoid_: godot-nx, NX port

**Homebrew**:
Software built with devkitPro (devkitA64 + libnx) that runs on a modded console or an emulator, never through official Nintendo channels.
_Avoid_: unofficial build, jailbreak build

**NRO**:
The homebrew executable format produced from a Godot ELF by `elf2nro`; the artifact an export produces.
_Avoid_: binary, ROM

**Template**:
Godot's exported-runtime binary for a platform (the usual Godot term); for the Switch port it is the ELF that becomes the NRO.
_Avoid_: player, runner

**First milestone**:
A 2D project with audio, controller and touch input, loading a `.pck` from the SD card, running on emulator and hardware.
_Avoid_: MVP, v1

**Portlibs**:
Prebuilt third-party libraries installed through `dkp-pacman` under `/opt/devkitpro/portlibs/switch`; the Switch port uses them only for the GPU stack.
_Avoid_: pacman packages, system libs

**GPU stack**:
devkitPro's mesa 20.1 (gallium nouveau driver) over devkitPro's libdrm_nouveau, exposing OpenGL 4.3 / GLES 3.1 through EGL on an `NWindow`. Unrelated to deko3d.
_Avoid_: driver, graphics backend

**Application mode / Applet mode**:
The two ways homebrew can be launched: as a full application (via title override, full RAM) or as an applet (via the album, restricted RAM). Memory budgets differ sharply.
_Avoid_: full mode, hbmenu mode

**Docked / Handheld**:
The console's two display modes (1080p on TV, 720p on the built-in screen); a running NRO must survive switching between them.
_Avoid_: TV mode, portable mode
