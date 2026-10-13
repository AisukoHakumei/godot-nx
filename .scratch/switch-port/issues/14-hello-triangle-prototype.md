# Prototype: libnx + EGL + GLES3 triangle with Godot's toolchain flags

Type: prototype
Status: open
Blocked by: 02, 05, 07
Map: ../map.md

## Question

Does a minimal NRO, built with the exact compiler/linker flags and link order the build-configuration ticket will adopt, create an EGL context on an `NWindow`, render a GLES3 triangle, log over `nxlink`, and survive a docked/handheld switch on both the chosen emulator and real hardware? Produces a throwaway `prototypes/switch-triangle/` and records what differed from the devkitPro example. Human runs it on hardware.

Context: the GPU-stack research names specific things to probe beyond the triangle: GL timer queries (`TODO: Support queries` stub in the devkitPro patch), `eglQuerySurface` width/height returning 0, `GL_NUM_PROGRAM_BINARY_FORMATS`, and whether an ES 3.x context or a GL 4.3 core context is the better host for Godot's GLES3 shaders; see [research/05-gpu-stack.md](../research/05-gpu-stack.md).
Context: run on Eden (Windows host, `-p` non-empty) and on hardware via nxlink; the emulator runs in application mode and hardware via hbmenu in applet mode, so test both memory situations; see [research/07-emulator.md](../research/07-emulator.md).
