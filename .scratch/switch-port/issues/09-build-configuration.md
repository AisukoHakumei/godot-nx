# Build configuration: detect.py and module set

Type: grilling
Status: open
Blocked by: 01, 02, 04, 05
Map: ../map.md

## Question

How is `platform/switch/detect.py` written: toolchain discovery from `DEVKITPRO`, compiler and linker flags (`-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE`, specs file), `pkg-config` from portlibs for the GPU stack, link order, target naming (`godot.switch.template_release.arm64.elf`)? Which modules are disabled (mono, openxr, webrtc, gdextension-dependent ones, etc.) and which defines are set (`UNIX_ENABLED`, `GLES3_ENABLED`, `EGL_STATIC`, no `VULKAN_ENABLED`)?

Context: build knobs implied by the libnx-runtime research: `use_volk=no`, `use_sowrap=no`, no `vulkan`, no `CRASH_HANDLER_ENABLED`, guard `<sys/xattr.h>`; see [research/04-libnx-runtime.md](../research/04-libnx-runtime.md).
Context: GL linking from the GPU-stack research: `EGL_STATIC`, do not compile `thirdparty/glad/egl.c`, stub `dlfcn.h` for `glad/gl.c`, link `-lEGL -lglapi -ldrm_nouveau -lnx`; see [research/05-gpu-stack.md](../research/05-gpu-stack.md).
