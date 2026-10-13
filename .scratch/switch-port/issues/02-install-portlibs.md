# Install the devkitPro Switch portlibs

Type: task
Status: open
Blocked by: (none)
Map: ../map.md

## Question

`/opt/devkitpro/portlibs/switch` currently holds only `bin/` (`pkg-config`, `cmake`); the GPU stack is absent. Already installed: `switch-tools 1.13.1-1` (`elf2nro`, `nacptool`), `switch-examples 20260201-1`, `switch-cmake`, `switch-pkg-config`. Install via `dkp-pacman` the GPU stack named by the GPU-stack research: `switch-mesa 20.1.0-5`, `switch-glad 0.1.27-1`, `switch-libdrm_nouveau 1.0.1-2` (or the `switch-dev` meta package), so the build-configuration decision and the hello-triangle prototype can link. HITL if `sudo` is needed: hand the human a checklist. Also install SCons 4.0+ for the host Python (`pip install --user scons`), which is absent in this WSL2 environment. Record installed package versions and the paths later tickets link against. If the GPU-stack research names extra packages, add them here.
