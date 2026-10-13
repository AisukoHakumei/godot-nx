# 05 — GPU stack: devkitPro mesa, EGL, and Godot's GLES3 needs

Research ticket for the Godot 4.7.2 → Nintendo Switch homebrew port (devkitA64 + libnx, Compatibility/GLES3 renderer only).
Godot source read from this worktree (master / 4.8-dev); the GLES3/EGL files were diffed against the `4.7.2-stable` tag and the differences are cosmetic for this ticket (see §2.6).
devkitPro portlibs are not installed locally; the `switch-mesa`, `switch-glad` and `switch-libdrm_nouveau` packages were downloaded from `pkg.devkitpro.org` (via `pacman -Sp`, pacman User-Agent) and inspected with `aarch64-none-elf-nm`. `/opt/devkitpro/examples/switch/graphics/opengl/*` (switch-examples 20260201-1) is installed locally and is byte-identical to the GitHub copy.

## Summary

1. The stack is **mesa 20.1.0-rc3 + a ~3.7k-line devkitPro patch**: gallium `nouveau` (nvc0) over devkitPro's `libdrm_nouveau` (a rewrite on libnx `nv*` services). It does **not** sit on deko3d; deko3d is a separate API [S1][S4][S12].
2. Exposed: **OpenGL 4.3 Core** is what devkitPro advertises and the examples request (nvc0 in mesa 20.1 is GL 4.5-capable); **OpenGL ES 3.1** (ES 3.2 is blocked because ASTC is gated off by the reported chipset 0x120); GLSL ≤4.50 in core contexts, ESSL 1.00/3.00/3.10 in ES contexts, and ESSL 300/310/320 also accepted inside a core context [S2][S6][S7][S8][S11].
3. EGL is **1.4** (`dpy->Version = 14`), one custom driver, `eglGetDisplay(EGL_DEFAULT_DISPLAY)` only, one colour format (RGBA8) × 7 depth/stencil formats, `EGL_WINDOW_BIT` only, native window = `NWindow*`; both `EGL_OPENGL_API` and `EGL_OPENGL_ES_API` bind; ES default if `eglBindAPI` is never called [S3][S9].
4. GL entry points are **not symbols**: there is no `libGL`; `libEGL.a` (22 MB, whole driver) + `libglapi.a` + `libGLESv2.a` (ES symbols only). Desktop GL must be fetched through `eglGetProcAddress` — this is exactly Godot's `gladLoadGL(&_egl_load_function_wrapper)` path, so **Godot can run against the static lib without GLAD's dlopen loader**, provided `EGL_STATIC` is defined, `thirdparty/glad/egl.c` is not compiled and `gl.c` gets a `dlfcn.h` stub (devkitA64 has none) [S13][S14][Godot-1][Godot-2].
5. Godot's generic `EGLManager` cannot be used unmodified: under `EGL_STATIC` it hard-calls `eglGetPlatformDisplay`/`eglCreatePlatformWindowSurface` (no Switch platform enum), links `eglSetBlobCacheFuncsANDROID` and `eglGetPlatformDisplayEXT` directly (both `static` in mesa 20.1, not exported), and `initialize()` fails unless the EGL client-extension string names the platform. A small Switch GL manager (like Stary2001's Godot 3 port) is the practical route [Godot-1][S13][S16][S17].
6. Biggest risks: (a) **no program binaries** on Switch (disk cache disabled → `NumProgramBinaryFormats = 0`) so Godot's shader cache is a no-op and every launch recompiles; (b) **no ASTC, no native ETC2** (chipset gating) — ship S3TC/BPTC textures; (c) GL timer queries are used every frame by Godot in GL mode but the Switch patch carries a `TODO: Support queries` stub (untested); (d) `eglQuerySurface` width/height returns 0 (open issue #1); (e) 2020-era mesa, single-threaded, TGSI-only, no NIR [S3][S5][S10][S11][S15].

## 1. What devkitPro's mesa exposes

### 1.1 What the stack actually is

- `switch-mesa` PKGBUILD: `pkgver=20.1.0`, `pkgrel=5`; the source tarball fetched is `mesa-20.1.0-rc3.tar.xz`; patches `switch-mesa-20.1.0-5.patch`, `gl_XML.py.patch`, `glX_XML.py.patch`; `depends=('switch-libdrm_nouveau')`; built with `/opt/devkitpro/meson-cross.sh switch ... -Db_ndebug=true`; `package()` appends `-ldrm_nouveau` after `-lEGL` in `egl.pc` [S1]. The pacman sync DB on this machine agrees: `switch-mesa 20.1.0-5`, build date 2025-07-04, depends `switch-libdrm_nouveau` [S13].
- The github.com/devkitPro/mesa repo's visible branch is `switch-20.1.0-rc3`; its README is upstream's, with the repo description "Switch port of the Mesa 3D graphics library" [S2].
- The patch adds a `horizon` host system and a `switch` EGL platform: `gallium_drivers = ['nouveau']`, `_platforms = ['switch']`, `-DHAVE_SWITCH_PLATFORM`, `-fno-exceptions -fno-rtti -fno-use-cxa-atexit`, no `dl`, **no zlib/zstd, no threads (`dep_thread = null_dep`), no LLVM** on horizon [S3, meson.build hunk].
- GPU access path: `src/gallium/winsys/nouveau/switch/nouveau_switch_winsys.c` → `nouveau_drm_new(0, &drm)` → `nouveau_device_new(...)` [S3]. devkitPro's `libdrm_nouveau` implements that on libnx: `nvInitialize()`, `nvFenceInit()`, `nvMapInit()`, `nvGpuInit()`, `nvdev->base.chipset = info->arch; // should be 0x120 (NVGPU_GPU_ARCH_GM200)`, `nvAddressSpaceCreate(...)` [S4]. libnx's `ioctl.h` documents `arch = 0x120 (NVGPU_GPU_ARCH_GM200)`, `impl = 0xB (NVGPU_GPU_IMPL_GM20B)` [S14-libnx].
- deko3d is a different thing: "deko3d is a brand new homebrew low level 3D graphics API targetting the Nvidia Tegra X1"; its FAQ says "switch-mesa provides a standard OpenGL API, while deko3d is a Switch homebrew specific API" and only deko3d's *shader compiler* "is based off mesa/nouveau sources" [S12]. Nothing in the mesa patch references deko3d.

### 1.2 EGL driver (`src/egl/drivers/switch/egl_switch.c`, added by the patch) [S3]

| Property | Value in source |
|---|---|
| EGL version | `dpy->Version = 14` → "1.4" |
| Client APIs | `EGL_OPENGL_BIT` if `_eglIsApiValid(EGL_OPENGL_API)`; `EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR` if `_eglIsApiValid(EGL_OPENGL_ES_API)` |
| Display extensions | `KHR_create_context`, `KHR_surfaceless_context` only |
| Configs | colour formats `{PIPE_FORMAT_R8G8B8A8_UNORM}` × depth `{NONE, S8_UINT, Z16, Z24X8, Z24_S8, Z32F, Z32F_S8X24}`; `SurfaceType = EGL_WINDOW_BIT` ("we only support creating window surfaces"); `RenderableType = EGL_OPENGL_BIT | ES_BIT | ES2_BIT | ES3_BIT_KHR`; `MinSwapInterval = 0`, `MaxSwapInterval = INT32_MAX` |
| Native types (`eglplatform.h`) | `EGLNativeDisplayType = void*`, `EGLNativeWindowType = void*` under `#elif defined(__SWITCH__)` (so `-D__SWITCH__` is mandatory when including EGL headers) |
| Window surface | casts `native_window` to `NWindow*`, checks `nwindowIsValid`, reads size via `nwindowGetDimensions`, allocates `NUM_BUFFERS` GPU buffers and `nwindowConfigureBuffer`s them; `eglSwapBuffers` → `nwindowQueueBuffer`; `eglSwapInterval` → `nwindowSetSwapInterval` |
| Context creation | `EGL_OPENGL_API`: core profile only honoured if `major > 3 || (major == 3 && minor >= 2)` (else falls through to `ST_PROFILE_DEFAULT` = compatibility); `EGL_OPENGL_ES_API`: major 1 → ES1, major 2 **or 3** → `ST_PROFILE_OPENGL_ES2` ("used for OpenGL ES 3.x too") |
| Driver API table | `Initialize, Terminate, CreateContext, DestroyContext, MakeCurrent, CreateWindowSurface, CreatePixmapSurface, CreatePbufferSurface, DestroySurface, SwapInterval, SwapBuffers, GetProcAddress` — no `QuerySurface` override, which matches the open width/height bug (§3) |
| `eglGetProcAddress` | `switch_get_proc_address` → `_glapi_get_proc_address(procname)` (libglapi) |
| `eglGetDisplay` | `_EGL_PLATFORM_SWITCH` is the native platform (`egl_native_platform = _platforms[0]`), so `eglGetDisplay(EGL_DEFAULT_DISPLAY)` works. `eglGetPlatformDisplay` is exported but `_eglGetPlatformDisplayCommon` has no `switch` case [S3][S16] |

The exported EGL symbols confirmed with `nm` on `libEGL.a`: `eglGetDisplay, eglGetPlatformDisplay, eglInitialize, eglBindAPI, eglChooseConfig, eglGetConfigAttrib, eglCreateWindowSurface, eglCreatePlatformWindowSurface, eglCreateContext, eglDestroyContext, eglDestroySurface, eglMakeCurrent, eglSwapBuffers, eglSwapInterval, eglQuerySurface, eglQueryString, eglGetError, eglTerminate, eglGetProcAddress, nouveau_switch_screen_create, _eglInitDriver`. **Not exported**: `eglSetBlobCacheFuncsANDROID`, `eglGetPlatformDisplayEXT`, `eglCreatePlatformWindowSurfaceEXT` — all three are `static` in mesa 20.1's `eglapi.c` and only reachable via `eglGetProcAddress` [S13][S16].

EGL client extension string (queried with `EGL_NO_DISPLAY`) in mesa 20.1: `EGL_EXT_client_extensions EGL_EXT_device_base EGL_EXT_device_enumeration EGL_EXT_device_query EGL_EXT_platform_base EGL_KHR_client_get_all_proc_addresses EGL_KHR_debug` plus `EGL_EXT_platform_device` and per-platform entries guarded by `HAVE_WAYLAND_PLATFORM` / `HAVE_X11_PLATFORM`; nothing is added for Switch [S17].

### 1.3 GL / GLES versions and GLSL

- devkitPro's announcement (libnx 1.4.0): "we now provide support for OpenGL 4.3 (Core Profile) ... OpenGL ES 2.x and 3.x are also additionally available" [S6]. The follow-up (switch-mesa 18.2β) notes "Improvements to EGL context creation support ... (involving breaking changes)" and that examples were "adapted to EGL breaking changes related to GL version selection during context creation" [S6b].
- Mesa 20.1 `docs/features.txt`: nvc0 is in the "all DONE" list for GL 3.0–4.5 / GLSL 1.30–4.50 and for **GLES 3.1**; the GLES 3.2 "all DONE" list is `i965/gen9+, radeonsi, virgl` only [S7].
- Compatibility profile: `_mesa_get_version()` caps `GLSLVersion` to `GLSLVersionCompat` unless `AllowHigherCompatVersion` (a driconf option that Switch has no way to set), and features.txt warns such drivers are "limited to OpenGL 3.0" without an explicit core request [S8][S7]. The devkitPro EGL driver only produces a core context when the profile mask says core **and** the version is ≥ 3.2 [S3]. ⇒ Godot must request `EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR = CORE` with major/minor ≥ 3.2 (the examples use 4.3).
- ES 3.2 gate: `compute_version_es2()` requires `KHR_texture_compression_astc_ldr` for 3.2 [S8]. nvc0's `is_format_supported` rejects ETC and ASTC layouts unless `chipset == 0x12b` or `class_3d == NVEA_3D_CLASS` [S11]; devkitPro's `libdrm_nouveau` sets `chipset = info->arch` = 0x120 [S4][S14-libnx]. Therefore the ES context should report **3.1** (inference from source; not measured on hardware).
- GLSL: in a core context mesa accepts desktop `#version` up to the context's version and, additionally, `300 es` (ARB_ES3_compatibility), `310 es` (ARB_ES3_1_compatibility) and `320 es` (ARB_ES3_2_compatibility, which "doesn't require ASTC or advanced blending") [S9][S10]. The official `textured_cube` example compiles `#version 320 es` shaders inside a GL 4.3 core context; `simple_triangle` uses `#version 330 core` [S5].
- Env-var configuration works because `os_get_option()` is plain `getenv()` [S18]; the examples' `setMesaConfig()` lists `MESA_NO_ERROR`, `EGL_LOG_LEVEL=debug`, `MESA_VERBOSE=all`, `NOUVEAU_MESA_DEBUG=1`, `NV50_PROG_OPTIMIZE=0`, `NV50_PROG_DEBUG=1`, `NV50_PROG_CHIPSET=0x120` [S5]. `MESA_GL_VERSION_OVERRIDE` / `MESA_GLES_VERSION_OVERRIDE` are also honoured by `version.c` [S8].

### 1.4 What the package ships (from the `.pkg.tar.zst`) [S13]

- Libraries: `libEGL.a` (22,478,644 bytes — the patch links the whole of libmesa/libgallium/libglsl/libnouveau/winsys into it via `objs_for_egl`), `libglapi.a` (987 KB), `libGLESv2.a` (161 KB, 358 `gl*` text symbols incl. `glGetStringi, glProgramBinary, glGetProgramBinary, glTexStorage3D, glFenceSync, glDebugMessageCallback, glBlendBarrier`), `libGLESv1_CM.a`. **No `libGL`** (glx disabled on horizon). All built as static `library()` by the patch [S3].
- pkg-config: `egl.pc` → `Requires: glapi`, `Libs: -L${libdir} -lEGL -ldrm_nouveau -lm`, `Cflags: -I${includedir} -DEGL_NO_X11`; `glesv2.pc` → `-lGLESv2`, `Requires: glapi`; `glapi.pc` → `-lglapi`. `OpenGLConfig.cmake` defines `OpenGL::GL`/`OpenGL::EGL` as `EGL;glapi;drm_nouveau`.
- Headers: `EGL/{egl,eglext,eglmesaext,eglextchromium,eglplatform}.h`, `GL/{gl,glcorearb,glext}.h`, `GLES2/*`, `GLES3/{gl3,gl31,gl32,gl3ext,gl3platform}.h`, `KHR/khrplatform.h`. `eglext.h` defines `EGL_ANDROID_blob_cache 1` and `EGL_EXT_platform_base 1` (prototypes only).
- `switch-glad 0.1.27-1` (depends on switch-mesa): glad **0.1.28**-generated, `--profile core --api gl=4.3 --no-loader --extensions GL_EXT_texture_compression_s3tc,GL_EXT_texture_filter_anisotropic`, plus a Switch-only `int gladLoadGL(void) { return gladLoadGLLoader((GLADloadproc)eglGetProcAddress); }`; ships `include/glad/glad.h` + `libglad.a` whose only undefined import is `eglGetProcAddress` [S14][S13].
- `switch-libdrm_nouveau 1.0.1-2`: "Interface between mesa/nouveau and Nintendo Switch Nvidia GPU driver"; headers `nouveau.h`, `nouveau_drm.h`, `nvif/*`; `libdrm_nouveau.a` [S4b].

## 2. What Godot 4.7's Compatibility renderer requires / probes

### 2.1 Build-system switches (how `GLES3_ENABLED`, `EGL_ENABLED`, `EGL_STATIC`, `GLAD_ENABLED` interact)

- `GLES3_ENABLED` is added by each `platform/*/detect.py` when `opengl3=yes` (linuxbsd l.555, android l.316 which also links `GLESv3` + `EGL`, windows l.518, macos l.308, ios, web) [Godot-3].
- `drivers/SCsub`: `if env["opengl3"]: SConscript("gl_context/SCsub"); SConscript("gles3/SCsub"); SConscript("egl/SCsub")` [Godot-4].
- `drivers/gl_context/SCsub`: **only** for `macos/windows/linuxbsd` it compiles `thirdparty/glad/gl.c`, defines `GLAD_ENABLED`; on linuxbsd it also compiles `glad/egl.c` and defines `EGL_ENABLED GLAD_GLES2`; on macos/windows `EGL_ENABLED GLAD_GLES2` only with `angle=yes` [Godot-4]. A new `switch` platform therefore gets none of this automatically and must choose its own set.
- `EGL_STATIC` is only set by windows/macos together with `ANGLE_ENABLED` (static ANGLE libs) [Godot-3]. Its effects in `drivers/egl/egl_manager.cpp` [Godot-1]:
  - `#define GLAD_EGL_VERSION_1_5 true`, `GLAD_EGL_EXT_platform_base 1` if the header defines it, and `extern "C"` declarations of `eglSetBlobCacheFuncsANDROID` and `eglGetPlatformDisplayEXT` with `KHRONOS_STATIC`.
  - `initialize()` skips `gladLoaderLoadEGL` entirely (`#if defined(GLAD_ENABLED) && !defined(EGL_STATIC)`), but still does `eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS)` and `ERR_FAIL`s if `_get_platform_extension_name()` is not in that string.
  - `_get_gldisplay_id()` always takes the `eglGetPlatformDisplay(_get_platform_extension_enum(), ...)` branch, then `eglInitialize`, `eglBindAPI(_get_platform_api_enum())`, `_gldisplay_create_context()`; with `EGL_ANDROID_blob_cache` defined it sets `has_blob_cache = true` and calls `eglSetBlobCacheFuncsANDROID` unconditionally (not exported by switch-mesa → link error, see §1.2).
  - `window_create()` calls `eglCreatePlatformWindowSurface` (exported by switch-mesa, but it routes through the same platform-less path).
  - `_gldisplay_create_context()` chooses a config with `RED/GREEN/BLUE ≥ 1, DEPTH 24` (or 8/8/8/8 + depth 24 when layered) and passes `_get_platform_context_attribs()`; the only in-tree EGL subclass (`GLManagerEGL_X11`) returns `EGL_CONTEXT_CLIENT_VERSION 3` and binds `EGL_OPENGL_ES_API` [Godot-5].
- Headers are indirected through per-platform `platform_gl.h` / `platform_egl.h`: linuxbsd includes `thirdparty/glad/glad/gl.h` with `GL_API_ENABLED`, `GLES_API_ENABLED`, `GLAD_GLES2` and `thirdparty/glad/glad/egl.h`; Android includes system `<GLES3/gl3.h>` + `<EGL/egl.h>/<EGL/eglext.h>` with only `GLES_API_ENABLED` and **no GLAD at all** (`ANDROID_ENABLED` replaces `EGL_ENABLED` in the `#if` guards); windows/macos under `ANGLE_ENABLED && EGL_STATIC` include ANGLE's `EGL/egl.h` with `KHRONOS_STATIC` [Godot-6].

### 2.2 GL function loading in `RasterizerGLES3::RasterizerGLES3()` [Godot-2]

```
#ifdef GLAD_ENABLED
  #ifdef EGL_ENABLED
    has_egl = true (EGL_STATIC) or (eglGetProcAddress != nullptr)
    if is_gles_over_gl(): gladLoadGL((GLADloadfunc)&_egl_load_function_wrapper)   // wrapper = eglGetProcAddress
    else:                 gladLoadGLES2((GLADloadfunc)&_egl_load_function_wrapper)
  #endif
  if still not loaded: gladLoaderLoadGL() / gladLoaderLoadGLES2()   // dlopen fallbacks
  ERR_FAIL_COND_MSG(!glad_loaded, "Error initializing GLAD.");
#endif
```
So with `GLAD_ENABLED + EGL_ENABLED + EGL_STATIC` Godot resolves every GL pointer through `eglGetProcAddress` and never reaches the dlopen fallback — the same mechanism as `switch-glad`'s `gladLoadGL` [S14]. Caveats found in `thirdparty/glad` (glad 2.0.8, `--api gl:compatibility=3.3,gles2=3.2 --loader`, extensions `GL_ARB_debug_output, GL_ARB_framebuffer_object, GL_ARB_get_program_binary, GL_EXT_framebuffer_blit/multisample/object, GL_OVR_multiview/2`) [Godot-7]:
- `gl.c` lines 2376–2541 (`#ifdef GLAD_GL`) and 2542–2731 (`#ifdef GLAD_GLES2`) contain the dlopen loaders and `#include <dlfcn.h>` unconditionally for non-Win32; `egl.c` likewise. devkitA64's newlib has no `dlfcn.h` (`/opt/devkitpro/devkitA64/aarch64-none-elf/include/dlfcn.h` does not exist). Options: compile `gl.c` with a stub `dlfcn.h` on the include path (dlopen returning NULL), or `-DGLAD_PLATFORM_WIN32=0` plus a patched copy; do not compile `egl.c` (EGL_STATIC makes it unnecessary).
- `switch-glad` is not a drop-in replacement: it is glad 0.1 (`gladLoadGLLoader`, `GLAD_GL_VERSION_4_3`), covers only desktop GL 4.3 core, and lacks the glad2 symbols Godot references (`gladLoadGL(GLADloadfunc)`, `gladLoadGLES2`, `GLAD_GL_ARB_debug_output`, `GLAD_VERSION_MAJOR`) [S14][Godot-2].
- Alternative with zero GLAD: the Android pattern — `platform_gl.h` includes `<GLES3/gl31.h>`, link `-lGLESv2`, `RasterizerGLES3::make_current(false)` (ES mode). This is the exact path Stary2001's Godot 3 Switch port took (`LIBS = EGL GLESv2 glapi drm_nouveau nx`, `EGL_CONTEXT_CLIENT_VERSION 3`) [S15].

### 2.3 Context mode and shader headers

- `RasterizerGLES3::make_current(bool p_gles_over_gl)` selects desktop-GL vs ES semantics; callers: linuxbsd/windows/macos pass `true` for `opengl3`, `false` for `opengl3_es`/ANGLE/Android/Web [Godot-8]. The GLX path requests **GL 3.3 core** (`GLX_CONTEXT_MAJOR/MINOR 3.3, CORE_PROFILE_BIT`) [Godot-9]; EGL path requests ES 3 (§2.1).
- `ShaderGLES3::_build_variant_code`: GL mode emits `#version 330\n#define USE_GLES_OVER_GL`; ES mode emits `#version 300 es` plus `precision highp sampler2D/samplerCube/sampler2DArray/sampler3D` [Godot-10]. Both are accepted by nouveau (GLSL 3.30 core; ESSL 3.00) [S7][S9].
- Desktop-only code behind `GL_API_ENABLED && is_gles_over_gl()`: `glClearDepth`, `glQueryCounter(GL_TIMESTAMP)` + `glGetQueryObjectui64v` (called every frame from `Utilities::capture_timestamp`), `glGetTexImage`, `GL_PROGRAM_POINT_SIZE`, `GL_TEXTURE_CUBE_MAP` seamless enable, `glDebugMessageControlARB` (GLAD only). All are GL 3.3-core functionality [Godot-11][Godot-12].

### 2.4 Extension / limit probing in `GLES3::Config` [Godot-13]

Enumerated via `glGetIntegerv(GL_NUM_EXTENSIONS)` + `glGetStringi(GL_EXTENSIONS, i)` (requires GL 3.0 / ES 3.0).

| Godot flag | GL mode (`is_gles_over_gl`) | ES mode | nouveau 20.1 (GM20B reported as 0x120) |
|---|---|---|---|
| `float_texture_supported` / `_linear` | assumed true | `GL_EXT_color_buffer_float`, `GL_OES_texture_float_linear` | both DONE for nvc0 [S7][S19] |
| `etc2_supported` | **false** (CPU decompress in Godot) | **true** (unconditional on non-web) | no native ETC on 0x120; mesa emulates via ARB_ES3_compatibility (CPU decode to RGBA8/R16) [S10][S11] |
| `s3tc_supported` | true | `GL_EXT_texture_compression_dxt1/s3tc` (ES2+ in mesa) | native DXT1/3/5 in nv50 format table [S11b][S19] |
| `rgtc_supported` | true | `GL_EXT_texture_compression_rgtc` (ES 3.0+) | GL 3.0 core, DONE |
| `bptc_supported` | `GL_ARB_texture_compression_bptc` / `GL_EXT_...` | same | GL 4.2 "all DONE: nvc0"; BC6H/BC7 in format table [S7][S11b] |
| `astc_*` | `GL_KHR_texture_compression_astc_ldr/hdr/sliced_3d`, `GL_OES_texture_compression_astc` | same | **absent** (chipset gate) [S11] |
| `srgb_framebuffer_supported` | true → `glDisable(GL_FRAMEBUFFER_SRGB)` | `GL_EXT_sRGB_write_control` | DONE (all GLES 3.0+ drivers) |
| `unorm16_texture_supported` | true | `GL_EXT_texture_norm16` | DONE (nvc0) [S7] |
| `support_anisotropic_filter` | `GL_EXT_texture_filter_anisotropic` | same | DONE (nvc0) [S7] |
| `msaa_supported` | true; `GL_MAX_SAMPLES` | same | nvc0 allows 0/1/2/4/8 samples [S11] |
| `multiview_supported` | `GL_OVR_multiview(2)` | same | not expected (XR only) |
| Android-only: `rt_msaa_*`, `external_texture_supported` | — | `GL_EXT_multisampled_render_to_texture`, `GL_OES_EGL_image_external_essl3` | compiled only under `ANDROID_ENABLED` |

Limits read: `GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, GL_MAX_TEXTURE_IMAGE_UNITS, GL_MAX_TEXTURE_SIZE, GL_MAX_VIEWPORT_DIMS, GL_MAX_VERTEX_ATTRIBS, GL_MAX_UNIFORM_BLOCK_SIZE (clamped 16K..1M), GL_MAX_VERTEX_OUTPUT_COMPONENTS, GL_MAX_FRAGMENT_INPUT_COMPONENTS, GL_MAX_SAMPLES` plus `GL_RENDERER` / `GL_VERSION` strings for vendor quirks (Adreno, PowerVR) [Godot-13]. Core ES3 features used throughout the driver: UBOs (`glBindBufferBase`, 100+ uses), `GL_TEXTURE_2D_ARRAY`, instancing (`glVertexAttribDivisor`, `glDraw*Instanced`), `glBlitFramebuffer`, `glMapBufferRange`, `glFenceSync/glClientWaitSync`, transform feedback (particles, mesh skinning), `glRenderbufferStorageMultisample`, `GL_RGB10_A2`, `GL_DEPTH24_STENCIL8`, `GL_HALF_FLOAT` [Godot-12].

### 2.5 Shader cache (`ShaderGLES3::_save_to_cache/_load_from_cache`) [Godot-10]

Uses `glGetProgramiv(GL_PROGRAM_BINARY_LENGTH)` + `glGetProgramBinary` / `glProgramBinary` (ES 3.0 core, `GL_ARB_get_program_binary` on desktop — included in Godot's glad set). A zero `program_size` is written as a 0-length entry and skipped on load, so a driver with no binary formats degrades to "compile every run" without errors. Also `EGLManager` registers `eglSetBlobCacheFuncsANDROID` when `EGL_ANDROID_blob_cache` is defined.

### 2.6 master vs 4.7.2-stable

`diff` of `drivers/egl/egl_manager.cpp`, `drivers/gles3/{rasterizer_gles3,shader_gles3,rasterizer_util_gles3}.cpp`, `drivers/gles3/storage/{config,utilities}.cpp`, `drivers/gl_context/SCsub`, `platform/linuxbsd/platform_gl.h`, `platform/linuxbsd/x11/gl_manager_x11_egl.cpp` against `4.7.2-stable` (raw from GitHub): identical for SCsub/platform_gl.h/gl_manager_x11_egl.cpp/rasterizer_util; master adds `current_window = nullptr` in `window_destroy`, a `WEB_ENABLED` guard around `etc2_supported` (4.7.2 sets `etc2_supported = true` in ES mode unconditionally), decal plumbing in `utilities.cpp`, compressed embedded shader code and a `multiview_supported` guard in `shader_gles3.cpp`, and `glBindTexture(target, 0)` in `rasterizer_gles3.cpp`. None change the EGL/GLAD/extension logic described above [Godot-14].

## 3. Missing or known-buggy on the devkitPro stack

1. **No program binaries ⇒ Godot shader cache is a no-op.** Mesa sets `NumProgramBinaryFormats = 1` only if `screen->get_disk_shader_cache(screen)` returns a cache [S10b]. `nouveau_disk_cache_create()` returns early when `disk_cache_get_function_identifier()` fails [S11c]; the Switch patch makes that function `return false` when `HAVE_DLADDR` is absent (no `dl` on horizon) [S3, disk_cache.h hunk]. Even otherwise, `disk_cache_create` needs `MESA_GLSL_CACHE_DIR`/`XDG_CACHE_HOME`/`getpwuid` paths [S10c]. Expect every launch to compile all GLES3 shader variants (Godot already tolerates this, §2.5), and plan a warm-up/preload. The patch also strips NIR (`PIPE_SHADER_IR_TGSI` only, `prefer_nir = false`) and the NIR serialisation paths [S3, nvc0_screen.c / st_shader_cache.c hunks].
2. **ASTC absent, ETC2 not native.** `nvc0_screen_is_format_supported` restricts ETC/ASTC to chipset `0x12b` or `NVEA` (GK20A), but devkitPro's libdrm_nouveau reports `0x120` [S11][S4]. GL mode: Godot already CPU-decompresses ETC2/ASTC (`need_decompress`) [Godot-15]; ES mode: Godot assumes ETC2 and mesa decodes it on the CPU (ARB_ES3_compatibility "Requirements for ETC2 emulation") [S10]. Export with S3TC/BPTC (both native on nvc0 [S11b]).
3. **GL queries**: the Switch patch wraps the `NVIF_CLASS_SW_GF100` object creation in `#if 0 // TODO: Support queries` [S3, nvc0_screen.c hunk]. In upstream 20.1 that object is only bound at screen init (no references from `nvc0_query_hw.c`) [S11][S11d], so occlusion/timestamp queries are probably unaffected, but Godot's per-frame `glQueryCounter(GL_TIMESTAMP)` in GL mode is a first thing to verify on hardware; ES mode never issues them [Godot-11].
4. **`eglQuerySurface(EGL_WIDTH/EGL_HEIGHT)` returns 0** — devkitPro/mesa issue #1, open since 2019-05-18, reporter "manually overrode it to 720p" [S20]. Godot's `EGLManager` only uses `eglQuerySurface` for the ANGLE orientation query (Windows) [Godot-1]; take sizes from `nwindowGetDimensions` / `nwindowSetDimensions` (defaults "usually 1280x720"; `dynamic_resolution` example sets 1920×1080 before EGL) [S14-libnx][S5].
5. **EGL 1.4 only; one RGBA8 colour format; window surfaces only; no `EGL_KHR_image`, no blob cache, no platform extensions** (§1.2). `nwindowSetSwapInterval(0)` only works with ≥3 buffers, otherwise the system enforces 1 [S14-libnx].
6. **Runtime environment constraints**: mesa built with no threads (`HAVE_PTHREAD` undefined → no glthread, no parallel shader compile), no zlib/zstd, `-fno-exceptions -fno-rtti -fno-use-cxa-atexit` ("newlib only supports 32 atexit functions and mesa wants to register around 200") [S3]. "It is not possible to use the libnx console and the GPU at the same time" — debug output must go to nxlink [S5b]. Applications are "several megabytes in size" (libEGL.a alone is 22 MB) [S12][S13].
7. **Age / maintenance**: code is mesa 20.1 (2020) with a 2025 rebuild; the public devkitPro/mesa repo has 1 open issue and no Switch-specific README [S2]. A community Mesa 26 (NVK) port exists but its own GL path "shows a black screen" and GL and NVK can't coexist in one process; the maintainer discussion (pacman-packages #439) was moved to a private repo [S21]. devkitPro forum: legacy GL functions such as `glRasterPos2i` crash because the software fallback "was disabled/removed in switch-mesa" — irrelevant to Godot's core-profile usage but shows unsupported paths fault rather than warn [S22].
8. Nothing on switchbrew.org documents OpenGL/mesa (the Homebrew Applications and Setting-up pages do not mention it); the authoritative docs are the switch-examples README and the libnx headers [S5b][S14-libnx].

## 4. Packages, link order, canonical init sequence

### 4.1 Packages (pacman names, versions in the dkp-libs DB on this machine) [S13][S1][S4b][S14]

| Package | Version | Role | Depends |
|---|---|---|---|
| `switch-mesa` | 20.1.0-5 | `libEGL.a` (driver), `libglapi.a`, `libGLESv2.a`, `libGLESv1_CM.a`, headers, `.pc`, `OpenGLConfig.cmake` | `switch-libdrm_nouveau` |
| `switch-libdrm_nouveau` | 1.0.1-2 | `libdrm_nouveau.a` (+`libdrm_nouveaud.a` debug), nouveau/nvif headers | — |
| `switch-glad` | 0.1.27-1 | optional GL 4.3-core loader (`libglad.a`, `glad/glad.h`); not usable by Godot's glad2 code (§2.2) | `switch-mesa` |
| `switch-glm` | — | only for the examples' math | — |

Install: `(dkp-)pacman -S switch-mesa switch-glad switch-glm` [S5b]; `switch-libdrm_nouveau` comes in as a dependency.

### 4.2 Link lines (from the installed examples' Makefiles) [S5c]

- Desktop GL via glad: `LIBS := -lglad -lEGL -lglapi -ldrm_nouveau -lnx` (`simple_triangle`, `gpu_console`; `-lm` added by `lenny`, `textured_cube`, `dynamic_resolution`).
- OpenGL ES via symbols: `LIBS := -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm` (`es2gears`, includes `<GLES2/gl2.h>`, never calls `eglBindAPI`).
- `LIBDIRS := $(PORTLIBS) $(LIBNX)`; `CFLAGS += $(INCLUDE) -D__SWITCH__` (required by mesa's `eglplatform.h`); `LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,...`; linker is `$(CXX)` when any `.cpp` exists [S5c]. `switch_rules` sets `PORTLIBS := $(PORTLIBS_PATH)/switch` and prepends `$(PORTLIBS)/bin` to `PATH` [S14-libnx-rules].
- Order rationale (static archives, single pass): `libEGL.a` has undefined `nouveau_*` (→ `libdrm_nouveau.a`) and `nv*`/`nwindow*`/`getenv` (→ `libnx.a`/newlib); `libglad.a` has undefined `eglGetProcAddress` (→ `libEGL.a`), so glad must precede EGL, EGL precede glapi/drm_nouveau, and `-lnx` last [S13]. pkg-config gives the same: `pkg-config --libs egl` → `-lEGL -ldrm_nouveau -lm` with `Requires: glapi` [S13].

### 4.3 Canonical init sequence (`graphics/opengl/simple_triangle/source/main.cpp`, verbatim order) [S5]

```c
#include <switch.h>
#include <EGL/egl.h>    // EGL library
#include <EGL/eglext.h> // EGL extensions
#include <glad/glad.h>  // glad library (OpenGL loader)

static bool initEgl(NWindow* win)
{
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);          // "Connect to the EGL default display"
    eglInitialize(s_display, nullptr, nullptr);
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) ...        // "Select OpenGL (Core) as the desired graphics API"
    static const EGLint framebufferAttributeList[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
        EGL_NONE };
    eglChooseConfig(s_display, framebufferAttributeList, &config, 1, &numConfigs);   // numConfigs == 0 → fail
    s_surface = eglCreateWindowSurface(s_display, config, win, nullptr);              // win is NWindow*
    static const EGLint contextAttributeList[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
        EGL_CONTEXT_MINOR_VERSION_KHR, 3,
        EGL_NONE };
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
    eglMakeCurrent(s_display, s_surface, s_surface, s_context);
    // failure paths: eglDestroySurface → eglTerminate
}

static void setMesaConfig()
{
    // Uncomment below to disable error checking and save CPU time (useful for production):
    //setenv("MESA_NO_ERROR", "1", 1);
    // Uncomment below to enable Mesa logging:
    //setenv("EGL_LOG_LEVEL", "debug", 1);
    //setenv("MESA_VERBOSE", "all", 1);
    //setenv("NOUVEAU_MESA_DEBUG", "1", 1);
    // Uncomment below to enable shader debugging in Nouveau:
    //setenv("NV50_PROG_OPTIMIZE", "0", 1);
    //setenv("NV50_PROG_DEBUG", "1", 1);
    //setenv("NV50_PROG_CHIPSET", "0x120", 1);
}

int main(int argc, char* argv[])
{
    setMesaConfig();                       // 1. env vars BEFORE any EGL call (mesa reads them with getenv)
    if (!initEgl(nwindowGetDefault()))     // 2. default NWindow is created by libnx before main()
        return EXIT_FAILURE;
    gladLoadGL();                          // 3. switch-glad: gladLoadGLLoader(eglGetProcAddress)
    sceneInit();                           // 4. shaders: "#version 330 core"
    padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
    while (appletMainLoop()) { padUpdate(&pad); ...; sceneRender(); eglSwapBuffers(s_display, s_surface); }
    sceneExit(); deinitEgl();              // eglMakeCurrent(NO_SURFACE/NO_CONTEXT) → eglDestroyContext → eglDestroySurface → eglTerminate
    return EXIT_SUCCESS;
}
```
`dynamic_resolution` additionally calls `nwindowSetDimensions(win, 1920, 1080)` on `nwindowGetDefault()` **before** `initEgl` (dimensions cannot change once buffers are registered) [S5][S14-libnx]. The ES variant (`es2gears`) is identical except no `eglBindAPI`, `EGL_CONTEXT_CLIENT_VERSION, 2`, and no glad [S5c].

### 4.4 Mapping onto Godot (implications, not yet decisions)

- Minimal `platform/switch` GL glue, mirroring the examples: `platform_egl.h` → `<EGL/egl.h>` + `<EGL/eglext.h>` (Android style), `platform_gl.h` → `thirdparty/glad/glad/gl.h` with `GL_API_ENABLED` (+`GLES_API_ENABLED`/`GLAD_GLES2` if ES fallback wanted); defines `GLES3_ENABLED GLAD_ENABLED EGL_ENABLED EGL_STATIC`; compile `thirdparty/glad/gl.c` with a `dlfcn.h` shim, skip `egl.c` and `drivers/egl/egl_manager.cpp` (or subclass it after guarding the four EGL_STATIC assumptions in §2.1); a `GLManagerSwitch` that does exactly §4.3 with `EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR=CORE, 4.3` (or 3.3 — anything ≥3.2 with the core bit), then `RasterizerGLES3::make_current(true)`.
- Fallback mode: ES 3.1 context (`EGL_CONTEXT_CLIENT_VERSION 3`, `eglBindAPI(EGL_OPENGL_ES_API)`), `-lGLESv2`, `make_current(false)`, GLAD optional — Godot 3's Switch port shipped this way [S15].
- Set `rendering/textures/vram_compression` to S3TC/BPTC for Switch exports; expect no shader cache; verify timestamp queries early; never rely on `eglQuerySurface` for size.

## Sources

Local Godot tree (this worktree, master `befe24c831`; 4.7.2-stable files fetched from `raw.githubusercontent.com/godotengine/godot/4.7.2-stable/`):
- [Godot-1] `drivers/egl/egl_manager.cpp`, `drivers/egl/egl_manager.h`
- [Godot-2] `drivers/gles3/rasterizer_gles3.cpp` (ll. 88–95, 232–332)
- [Godot-3] `platform/linuxbsd/detect.py` l.555, `platform/android/detect.py` ll.304–317, `platform/windows/detect.py` ll.518–535, `platform/macos/detect.py` ll.308–315
- [Godot-4] `drivers/SCsub`, `drivers/gl_context/SCsub`, `drivers/egl/SCsub`, `drivers/gles3/SCsub`
- [Godot-5] `platform/linuxbsd/x11/gl_manager_x11_egl.cpp`
- [Godot-6] `platform/linuxbsd/platform_gl.h`, `platform/linuxbsd/platform_egl.h`, `platform/android/platform_gl.h`, `platform/android/platform_egl.h`, `platform/windows/platform_egl.h`, `platform/windows/platform_gl.h`, `platform/macos/platform_egl.h`, `platform/web/platform_gl.h`
- [Godot-7] `thirdparty/glad/glad/gl.h` (header comment), `thirdparty/glad/gl.c` ll.2376–2731, `thirdparty/glad/egl.c` ll.274–420, `thirdparty/README.md` §glad (2.0.8)
- [Godot-8] `RasterizerGLES3::make_current` callers: `platform/linuxbsd/x11/display_server_x11.cpp` ll.7200–7288, `platform/linuxbsd/os_linuxbsd.cpp` l.1341, `platform/android/display_server_android.cpp` l.865, `platform/web/display_server_web.cpp` l.1157, `platform/windows/display_server_windows.cpp` ll.8394–8405; `main/main.cpp` ll.2390–2395, 2585–2613
- [Godot-9] `platform/linuxbsd/x11/gl_manager_x11.cpp` ll.186–188
- [Godot-10] `drivers/gles3/shader_gles3.cpp` ll.168–240, 555–700
- [Godot-11] `drivers/gles3/storage/utilities.cpp` ll.44–75, 333–400; `drivers/gles3/rasterizer_util_gles3.cpp`
- [Godot-12] grep of `drivers/gles3/**` for GL tokens/functions (UBO, texture arrays, instancing, sync, TF in `particles_storage.cpp`, `mesh_storage.cpp`)
- [Godot-13] `drivers/gles3/storage/config.cpp`
- [Godot-14] `diff -u` of the listed files vs `4.7.2-stable`
- [Godot-15] `drivers/gles3/storage/texture_storage.cpp` ll.750–980 (`need_decompress`)

devkitPro / switchbrew / mesa (all fetched 2026-10-13):
- [S1] `switch-mesa` PKGBUILD — https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mesa/PKGBUILD (dir listing: https://github.com/devkitPro/pacman-packages/tree/master/switch/mesa)
- [S2] https://github.com/devkitPro/mesa (branch `switch-20.1.0-rc3`, issues list https://github.com/devkitPro/mesa/issues?q=is%3Aissue)
- [S3] `switch-mesa-20.1.0-5.patch` — https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mesa/switch-mesa-20.1.0-5.patch (hunks: `meson.build`, `meson_options.txt`, `include/EGL/eglplatform.h`, `src/egl/drivers/switch/egl_switch.c`, `src/egl/main/egldisplay.{c,h}`, `src/egl/meson.build`, `src/gallium/winsys/nouveau/switch/nouveau_switch_winsys.c`, `src/gallium/drivers/nouveau/nouveau_screen.c`, `src/gallium/drivers/nouveau/nvc0/nvc0_screen.c`, `src/mapi/*/meson.build`, `src/mesa/state_tracker/st_shader_cache.c`, `src/util/disk_cache.h`, `src/util/os_misc.c`)
- [S4] devkitPro `libdrm_nouveau` `source/nouveau.c` — https://raw.githubusercontent.com/devkitPro/libdrm_nouveau/master/source/nouveau.c (ll.149–200)
- [S4b] `switch-libdrm_nouveau` PKGBUILD — https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/libdrm_nouveau/PKGBUILD
- [S5] switch-examples `graphics/opengl/simple_triangle/source/main.cpp` — https://github.com/switchbrew/switch-examples/blob/master/graphics/opengl/simple_triangle/source/main.cpp (identical to `/opt/devkitpro/examples/switch/graphics/opengl/simple_triangle/source/main.cpp`); `dynamic_resolution/source/main.cpp` ll.515–523, `textured_cube/source/main.cpp` ll.129–233 (local copies)
- [S5b] switch-examples `graphics/opengl/README.md` — https://github.com/switchbrew/switch-examples/blob/master/graphics/opengl/README.md
- [S5c] switch-examples Makefiles — https://raw.githubusercontent.com/switchbrew/switch-examples/master/graphics/opengl/simple_triangle/Makefile and `/opt/devkitpro/examples/switch/graphics/opengl/{simple_triangle,es2gears,textured_cube,dynamic_resolution,lenny,gpu_console}/Makefile` l.62; `es2gears/source/main.c` ll.52, 108–160
- [S6] devkitPro forum, "libnx 1.4.0 and a huge surprise!" — https://devkitpro.org/viewtopic.php?t=8780
- [S6b] devkitPro forum, "libnx 1.4.1, switch-mesa 18.2β, switch-examples 20180918" — https://devkitpro.org/viewtopic.php?f=13&t=8784
- [S7] Mesa 20.1.0 `docs/features.txt` — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/docs/features.txt
- [S8] Mesa 20.1.0 `src/mesa/main/version.c` — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/mesa/main/version.c (ll.58–59, 495–600)
- [S9] Mesa 20.1.0 `src/compiler/glsl/glsl_parser_extras.cpp` ll.236–262 — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/compiler/glsl/glsl_parser_extras.cpp
- [S10] Mesa 20.1.0 `src/mesa/state_tracker/st_extensions.c` ll.1494–1526 (ETC2 emulation), ll.1668–1690 (ES3_2_compatibility), l.1122 (`AllowHigherCompatVersion`) — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/mesa/state_tracker/st_extensions.c
- [S10b] same file ll.466–468 (`NumProgramBinaryFormats`)
- [S10c] Mesa 20.1.0 `src/util/disk_cache.c` ll.223–275 — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/util/disk_cache.c
- [S11] Mesa 20.1.0 `src/gallium/drivers/nouveau/nvc0/nvc0_screen.c` (`nvc0_screen_is_format_supported`, ll.1105–1115 `nvsw`) — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/gallium/drivers/nouveau/nvc0/nvc0_screen.c
- [S11b] Mesa 20.1.0 `src/gallium/drivers/nouveau/nv50/nv50_formats.c` ll.221–257 — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/gallium/drivers/nouveau/nv50/nv50_formats.c
- [S11c] Mesa 20.1.0 `src/gallium/drivers/nouveau/nouveau_screen.c` (`nouveau_disk_cache_create`) — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/gallium/drivers/nouveau/nouveau_screen.c
- [S11d] Mesa 20.1.0 `src/gallium/drivers/nouveau/nvc0/nvc0_query_hw.c`, `nvc0_context.c`, `nvc0_screen.h` (no `nvsw`/`SUBC_SW` users outside screen init)
- [S12] https://github.com/devkitPro/deko3d (README / FAQ)
- [S13] Package archives downloaded from `pkg.devkitpro.org/packages/` (`switch-mesa-20.1.0-5-any.pkg.tar.zst`, `switch-glad-0.1.27-1-any.pkg.tar.xz`, `switch-libdrm_nouveau-1.0.1-2-any.pkg.tar.xz`), file lists, `.PKGINFO`, `egl.pc`/`glesv2.pc`/`glapi.pc`, `OpenGLConfig.cmake`, `include/EGL/eglext.h`, and `aarch64-none-elf-nm -g --defined-only/--undefined-only` on `libEGL.a`, `libglapi.a`, `libGLESv2.a`, `libglad.a`; `pacman -Si switch-mesa switch-glad switch-libdrm_nouveau`
- [S14] https://github.com/devkitPro/switch-glad — `include/glad/glad.h` header (generator command line), `source/glad.c` l.1424, `Makefile`; `switch-glad` PKGBUILD https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/glad/PKGBUILD
- [S14-libnx] `/opt/devkitpro/libnx/include/switch/display/native_window.h` ll.51–132 (`nwindowGetDefault`, `nwindowGetDimensions`, `nwindowSetDimensions`, `nwindowSetSwapInterval`), `/opt/devkitpro/libnx/include/switch/nvidia/ioctl.h` ll.71–72 (libnx 4.12.0-1); online copy https://switchbrew.github.io/libnx/native__window_8h.html
- [S14-libnx-rules] `/opt/devkitpro/libnx/switch_rules`
- [S15] Stary2001 Godot 3.5.2 Switch port — https://github.com/Stary2001/godot (branch `3.5.2-stable_switch`): `platform/switch/detect.py` ll.248–256, `platform/switch/context_gl_switch_egl.cpp` ll.53–100, `platform/switch/os_switch.cpp`; fork https://github.com/PuntoScene/godot-nx
- [S16] Mesa 20.1.0 `src/egl/main/eglapi.c` ll.364–447 (`eglGetDisplay`, `_eglGetPlatformDisplayCommon`), ll.424, 1009 (`static ... eglGetPlatformDisplayEXT/eglCreatePlatformWindowSurfaceEXT`), l.2610 (`static ... eglSetBlobCacheFuncsANDROID`) — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/egl/main/eglapi.c ; `eglentrypoint.h` ll.21–80
- [S17] Mesa 20.1.0 `src/egl/main/eglglobals.c` ll.69–95 — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/egl/main/eglglobals.c
- [S18] Mesa 20.1.0 `src/util/os_misc.c` ll.120–123 (`os_get_option` → `getenv`) — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/util/os_misc.c
- [S19] Mesa 20.1.0 `src/mesa/main/extensions_table.h` (API columns for `EXT_texture_compression_s3tc` ES2, `EXT_texture_norm16` ES 3.1, `EXT_sRGB_write_control` ES 3.0, `KHR_texture_compression_astc_ldr`, `ARB_get_program_binary` GL only) — https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-20.1.0/src/mesa/main/extensions_table.h
- [S20] https://github.com/devkitPro/mesa/issues/1 — "eglQuerySurface gives zero values for WITH/HEIGHT" (open)
- [S21] https://github.com/devkitPro/pacman-packages/issues/439 — "switch-mesa: Mesa 26 with Vulkan (NVK), how would you want it packaged?" (content now moved to an inaccessible repo; details from search snippets only)
- [S22] devkitPro forum, "Legacy OpenGL code (glBegin/glEnd) on Switch" — https://devkitpro.org/viewtopic.php?f=42&t=9152 (via search snippet)
