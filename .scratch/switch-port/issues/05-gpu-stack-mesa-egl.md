# GPU stack: devkitPro mesa, EGL, and Godot's GLES3 needs

Type: research
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

What OpenGL and OpenGL ES versions does devkitPro's mesa (nouveau over deko3d) expose through EGL on an `NWindow`, which extensions Godot 4.7's Compatibility renderer requires or probes (`drivers/gles3`), which of those are missing or buggy on this stack, and which portlib packages and link order are needed (`EGL`, `glapi`, `drm_nouveau`, `nx`)? Include the canonical EGL init sequence from the devkitPro `graphics/opengl` examples. Cite mesa source, libnx, and the examples.

## Answer

**The stack is mesa 20.1.0-rc3 plus a ~3.7k-line devkitPro patch**: gallium `nouveau` (nvc0) over devkitPro's `libdrm_nouveau`, a rewrite on libnx `nv*` services. It does not sit on deko3d (deko3d is a separate API; the ticket premise and the glossary were wrong and are corrected).

Exposed: **OpenGL 4.3 Core** (what devkitPro advertises and the examples request) and **OpenGL ES 3.1** (ES 3.2 blocked because ASTC is gated off by the reported chipset 0x120). GLSL up to 4.50 in core contexts; ESSL 1.00/3.00/3.10 in ES contexts, and ESSL 300/310/320 also accepted inside a core context. EGL 1.4, `eglGetDisplay(EGL_DEFAULT_DISPLAY)` only, RGBA8 × 7 depth/stencil configs, `EGL_WINDOW_BIT` only, native window is `NWindow*`, both `EGL_OPENGL_API` and `EGL_OPENGL_ES_API` bind.

GL entry points are not symbols: no `libGL`; `libEGL.a` (whole driver, 22 MB) + `libglapi.a` + `libGLESv2.a` (ES symbols only). Desktop GL is fetched via `eglGetProcAddress`, which is exactly Godot's `gladLoadGL(&_egl_load_function_wrapper)` path, so **Godot runs against the static lib without GLAD's dlopen loader** provided `EGL_STATIC` is defined, `thirdparty/glad/egl.c` is not compiled, and `gl.c` gets a `dlfcn.h` stub.

Godot's generic `EGLManager` cannot be used unmodified: under `EGL_STATIC` it hard-calls `eglGetPlatformDisplay`/`eglCreatePlatformWindowSurface` (no Switch platform enum) and links `eglSetBlobCacheFuncsANDROID`/`eglGetPlatformDisplayEXT`, both `static` in mesa 20.1. **Write a small Switch GL manager** (as Stary2001's port did).

Packages (not yet installed; pacman only has `switch-tools 1.13.1-1`, `switch-examples 20260201-1`, `switch-cmake`, `switch-pkg-config`): `switch-mesa 20.1.0-5`, `switch-glad 0.1.27-1`, `switch-libdrm_nouveau 1.0.1-2`. Link: `-lEGL -lglapi -ldrm_nouveau -lnx`. Canonical init sequence from `switch-examples/graphics/opengl/simple_triangle` is in the findings, including the `setMesaConfig` env-var trick.

Biggest risks for the renderer:
1. No program binaries (disk cache disabled, `NumProgramBinaryFormats = 0`): Godot's shader cache is a no-op, every launch recompiles.
2. No ASTC, no native ETC2 (chipset gating): ship S3TC/BPTC textures.
3. GL timer queries, which Godot uses every frame, hit a `TODO: Support queries` stub in the Switch patch (untested).
4. `eglQuerySurface` width/height returns 0 (devkitPro/mesa issue #1).
5. 2020-era mesa, single-threaded, TGSI-only, no NIR.

Findings: [research/05-gpu-stack.md](../research/05-gpu-stack.md) (branch `research/gpu-stack`, commit f576b09f9b).
