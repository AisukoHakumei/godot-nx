# Vulkan for Godot 4.7 RenderingDevice on Nintendo Switch homebrew (devkitPro / libnx / Atmosphère)

Researched 2026-10-13. Repo: `/home/aisuko/gamedev/godot-nx` (Godot 4.7.2-stable, branch `switch-4.7`). "Verified" = read in a primary source at the URL given. "Inference" = my reading across sources; labelled as such.

## 0. Bottom line

- **Official devkitPro portlibs: no Vulkan.** `switch-mesa` is still Mesa 20.1.0-rc3 (`pkgver=20.1.0`, `pkgrel=5`, last package commit 2025-07-04), GL/GLES/EGL only. A community request to package Mesa 26 + NVK (devkitPro/pacman-packages #439, late Sept 2026) was moved to a private repo; nothing has been published.
- **Community Vulkan does exist and is active (all < 5 months old):** Mesa 26.2.x with NVK on a native Horizon backend (libnx `nv*` services, no DRM, no `libdrm_nouveau`), loaderless static `libvulkan.a`, `VK_KHR_surface` + `VK_NN_vi_surface` WSI on a libnx `NWindow`. Two lineages: **danfromtico/mesa-switch** (+ NaGaa95, MIT, Mesa 26.2.4, pushed 2026-10-13) and **PalindromicBreadLoaf/nxvk** (GPL-2.0-or-later additions, Mesa 26.2.2, pushed 2026-10-13); an earlier **HayatoG/switch-nvk** (June 2026) is dormant.
- **Reported Vulkan level on GM20B:** 1.3 in Aug–Sep 2026 builds (Godot log "Vulkan 1.3.354"; #439 says "Vulkan 1.3"), 1.4 in current source (both forks report 1.4 for Maxwell-A+ SoCs since host-image-copy was enabled on SoCs). Not conformant; upstream NVK explicitly excludes Tegra.
- **Godot's RenderingDevice minimums are met on paper and empirically:** a Godot 4.5.1 Forward Mobile build (Android `libgodot_android.so` behind a JNI shim) initialised and rendered on "NVIDIA Tegra X1 (GM20B) (NVK GM20B)" (GBAtemp, 2026-08-26), with "Mesa driver" visual glitches. Ozuba's native Godot 4.7-rc port claims both Forward+ and Mobile boot on NVK (self-reported, 0 stars, no media).
- **Presenting:** the driver provides a real `VK_KHR_swapchain` for an `NWindow` through `vkCreateViSurfaceNN`; 3 block-linear zero-copy scanout images, FIFO and IMMEDIATE modes, NvMultiFence-based acquire/present. No offscreen+manual-present path is needed.

## 1. What the local notes say (read-only, `git show HEAD:…`)

`.scratch/switch-port/research/03-prior-art.md` (researched 2026-10-13):

- §Summary 1: Ozuba/godot `switch-platform` is "Godot 4.7-rc, Jun–Sep 2026, self-described 'heavily vibecoded', builds and boots; GLES3 + Vulkan/NVK".
- §Summary 4 and §Not worth taking: recommends lifting Ozuba's platform layer but **"drop its Vulkan/NVK and Mesa-26 'unified SDK' dependency"**; the Vulkan stack "requires the non-portlib Mesa 26.2 'unified SDK' (danfromtico/mesa-switch, `build-unified.sh`, MSYS2 toolchain), `NVK_I_WANT_A_BROKEN_VULKAN_DRIVER`, title takeover, FIFO-only vsync, single-threaded NAK. Keep the `rendering_context_driver_vulkan_switch.*` files out of the initial port."
- §5 (Ozuba): repo https://github.com/Ozuba/godot, "not a GitHub fork (created 2026-06-10, last push 2026-09-12, 0 stars); branches `master`, `switch-platform` (default), `switch-platform-nvk`. `version.py` = 4.7.0-rc." Port commits include `ded7c60559` "Initial proof of concept vulkan port", `2b77d2ec93` "Testing with new nvk compilation", `e5c78a258b` "Partial working state, restored servers to upstream". `detect.py` sets `vulkan=True`, `use_volk=False`, links `EGL GLESv2 glapi vulkan vulkan_util mesa_util mesa_util_c11 mesa_util_simd blake3 expat z zstd nx m`; `mesa_sdk_path` points at `mesa-26.2.1-switch-unified-horizon-sdk/opt/devkitpro/portlibs/switch` built from **danfromtico/mesa-switch** ("Based on Mesa 26.2.4", "NVK, built loaderless", "Gallium NVC0 by default, or Zink layered on top of NVK", selector `MESA_SWITCH_GL_DRIVER=zink|nvc0`). "Vulkan path: `RenderingContextDriverVulkanSwitch` uses `VK_NN_VI_SURFACE_EXTENSION_NAME` and `vkCreateViSurfaceNN` on the nwindow, no validation layers; `godot_switch.cpp` sets `NVK_I_WANT_A_BROKEN_VULKAN_DRIVER=1`, `RAYON_NUM_THREADS=1`, `MESA_SHADER_COMPILER_THREADS=1`, `NVK_DEBUG=no_compression`, `MESA_SHADER_CACHE_DIR=sdmc:/switch/godot`. README limitations: NVK WSI is FIFO-only; NVK needs title takeover ('boot as application'), not applet mode."
- §Summary 8: "Ozuba only ran GLES3 through Mesa 26.2 (zink-over-NVK or nvc0) from a community Mesa port, not from portlibs."
- Sources list: https://github.com/danfromtico/mesa-switch (README), Ozuba raw files incl. `platform/switch/rendering_context_driver_vulkan_switch.cpp`.

`.scratch/switch-port/research/05-gpu-stack.md`:

- §Summary 1–2: stack is "mesa 20.1.0-rc3 + a ~3.7k-line devkitPro patch", gallium nouveau (nvc0) over devkitPro `libdrm_nouveau`; exposes OpenGL 4.3 Core / OpenGL ES 3.1; no Vulkan mentioned for the portlib.
- §Age/maintenance (item 7): "A community Mesa 26 (NVK) port exists but its own GL path 'shows a black screen' and GL and NVK can't coexist in one process; the maintainer discussion (pacman-packages #439) was moved to a private repo [S21]." [S21] = https://github.com/devkitPro/pacman-packages/issues/439 "switch-mesa: Mesa 26 with Vulkan (NVK), how would you want it packaged?" (content from search snippets only).
- [S2] https://github.com/devkitPro/mesa branch `switch-20.1.0-rc3`; [S1] PKGBUILD pkgver 20.1.0-5.

## 2. Question A — Does any Mesa build for Switch homebrew ship NVK?

### A.1 Official devkitPro: no

| Fact | Source |
|---|---|
| `switch-mesa` PKGBUILD: `pkgver=20.1.0`, `pkgrel=5`, `depends=('switch-libdrm_nouveau')` | https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mesa/PKGBUILD (fetched 2026-10-13) |
| Last commit touching `switch/mesa`: `c7876f72c1` "switch-mesa: 20.1.0-5", 2025-07-04 | GitHub API `repos/devkitPro/pacman-packages/commits?path=switch/mesa` |
| devkitPro/mesa has one branch, `switch-20.1.0-rc3`, last updated May 16 2020 (fincs) | https://github.com/devkitPro/mesa/branches |
| Issue #439 "switch-mesa: Mesa 26 with Vulkan (NVK), how would you want it packaged?": opener states "switch-mesa is 20.1.0-rc3 and GL only. I've ported Mesa 26.2.3 to Horizon, NVK included, so homebrew can use Vulkan 1.3"; "Sascha Willems' Vulkan samples (everything but ray tracing, run as one NRO): 79 of 84 pass"; "SDL3's SDL_GPU runs on it"; Mesa 26's own GL "builds and presents, but shows a black screen. So as it stands this can't replace switch-mesa 20.1"; proposes a separate `switch-mesa-vulkan` package. Posted ~15 days before the search (late Sept 2026). | Search-engine snippets of https://github.com/devkitPro/pacman-packages/issues/439 |
| The issue page now shows only "This issue has been moved to a repository you don't have access to." | https://github.com/devkitPro/pacman-packages/issues/439 (fetched 2026-10-13) |

Inference: devkitPro has not accepted or published anything; outcome of the private discussion is unknown.

### A.2 Community Mesa 26 + NVK ports (all verified via GitHub API on 2026-10-13)

| Repo | Created | Last push | Stars/forks | Mesa | License of port | Notes |
|---|---|---|---|---|---|---|
| https://github.com/danfromtico/mesa-switch | 2026-09-10 | 2026-10-13 | 22 / 15 | 26.2.4 (commit `2330d00771` "mesa: update Horizon port to 26.2.4", 2026-10-06) | MIT (README) | The "unified Mesa Horizon SDK" Ozuba builds against. Native Horizon backend `src/nouveau/horizon/` (`nouveau_horizon_{device,memory,vm,channel,runtime,gm20b}.c`), `src/nouveau/vulkan/nvkmd/switch/` (`nvkmd_switch_{pdev,dev}.c`), WSI `src/vulkan/wsi/wsi_common_switch.c`, `src/nouveau/vulkan/nvk_switch_wsi.h`. README: "NVK, built loaderless" — app links `libvulkan.a`, no loader/ICD; explicitly unsupported `VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`, `VK_EXT_map_memory_placed` (no POSIX fds / DMA-BUF on Horizon); "no external libdrm_nouveau and no patched libnx ABI"; GL = nvc0 by default or Zink-on-NVK (`MESA_SWITCH_GL_DRIVER`); build scripts `build-switch.sh` (NVK only, Docker), `build-opengl.sh`, `build-unified.sh` (needs MSYS2 toolchain + Rust `aarch64-unknown-linux-gnu`). Recent commits: 2026-10-13 "horizon: runtime shutdown also closes the devices' cached buffers"; 2026-10-06 "nvk: fix Horizon depth-stencil memory bindings", "horizon: fix GPFIFO submission capacity accounting"; 2026-09-28 merge of NaGaa95 "Mesa 26.2.3, NVK and NAK fixes, shared shader cache". |
| https://github.com/NaGaa95/mesa-switch (fork of the above) | 2026-09-11 | 2026-10-06 | 5 / 2 | 26.2.x | MIT | PR #1 into danfromtico (opened 2026-09-16, merged 2026-09-17, 18 commits): bump to 26.2.2, cache-coherency fixes, "Fixes a fragment-shader hang where an LDG instruction feeds a hardware loop" on GM20B, Zink lazy descriptors, shader cache keyed on build ID. https://github.com/danfromtico/mesa-switch/pull/1 |
| https://github.com/etonedemid/mesa-switch | 2026-09-26 | 2026-09-27 | 0 / 0 | 26.2.3 | — | Description: "Mesa 26.2.3 for the Nintendo Switch (Horizon homebrew): NVK Vulkan 1.3 and nouveau GL/EGL on libnx". 6 commits. Inference: same lineage/timeframe as issue #439 (26.2.3, "Vulkan 1.3"); not proven to be the #439 author. |
| https://github.com/autorunhq/switch-dev | 2026-09-28 | 2026-10-13 | 0 / 0 | mesa-switch @ `0ff52079` (= danfromtico HEAD of 2026-10-13) | — | Docker image `ghcr.io/autorunhq/switch-dev` (replaces devkitPro Mesa 20.1 + libdrm_nouveau with Mesa 26 nvc0/Zink + loaderless NVK) and `ghcr.io/autorunhq/switch-dev-legacy-vulkan` (keeps devkitPro GL 20.1, adds NVK Vulkan only; tag example `legacy-vulkan-2026.10.12`). libnx pinned at `146c3d14` (newer than 4.12.0). |
| https://github.com/PalindromicBreadLoaf/nxvk | 2026-07-04 | 2026-10-13 | 24 / 9 | 26.2.2 (README) | New files GPL-2.0-or-later; README warns a statically linked app is a combined GPL work | README (`switch/README.md`): "Vulkan: Version 1.4"; 15 instance extensions incl. `VK_KHR_surface`, `VK_NN_vi_surface`, `VK_EXT_headless_surface`, `VK_EXT_debug_utils`; 178 device extensions (88 KHR, 82 EXT); OpenGL 4.5 core / ES 3.2 via Zink + EGL over libnx `NWindow`; swapchain images "scan out zero-copy in block-linear format (kind=0xfe)", recommended `minImageCount = 3`; sparse binding off; DRM-dependent extensions advertised but no-ops; `textureCompressionASTC_LDR` = VK_TRUE; "Device creation fails unless the app sets `NVK_I_WANT_A_BROKEN_VULKAN_DRIVER`"; link with `--whole-archive -u vk_icdGetInstanceProcAddr`; `nvk_compat.c` newlib shims. Recent commits: 2026-10-13 "nvk: propagate the SLM drain failure", 2026-09-18 "Support VK_EXT_host_image_copy on SoCs". Referenced by Flycast issue #2409 (opened 2026-07-18; Zink GL added 2026-08-10). https://github.com/flyinghead/flycast/issues/2409 |
| https://github.com/StevensND/nxvk | 2026-10-02 | 2026-10-02 | 2 / 0 | fork of nxvk | GPL | Same README. |
| https://github.com/HayatoG/switch-nvk | 2026-05-27 | 2026-06-26 | 19 / 5 | directory `pristine-25.0.7/src` (inference: Mesa 25.0.7) | GPL-2.0-or-later (relicensed from AGPL 2026-06-26) | Own WSI: branch `switch-port/nvk-wsi` = `VK_NN_vi_surface` swapchain over `nwindow` (acquire/render/present/VI compositor); `switch-port/wsi-zero-copy` = block-linear `kind=0xfe` scanout, present time "~9.4 ms to microseconds"; commit 2026-06-20 "WSI: triple-buffer the nwindow swapchain (minImageCount 2 -> 3)". GBAtemp thread 2026-06-05 "So... Dan didn't want to reveal his Vulkan, so I made mine." (https://gbatemp.net/threads/so-dan-didnt-want-to-reveal-his-vulkan-so-i-made-mine.682214/): users report black screens and 7–17 fps in a Dusklight port; community members claim the repos re-release danfromtico's then-private files (unverified claims). Dormant since June. Fork: https://github.com/eriqueh2690-dev/switch-nvk9852 (2026-10-04). |
| https://github.com/D3fau4/mesa-nvk-horizon | 2026-07-24 | 2026-09-10 | 0 / 0 | pinned checkout | MIT | Self-described "unsupported", "written entirely by an AI coding assistant as a personal experiment"; goal: kernel layer that "speaks Horizon natively (NvMap, NvAddressSpace, NvGpuChannel, GPFIFO, NvFence, syncpoints) instead of emulating the Linux nouveau DRM uAPI underneath NVK"; "The Vulkan half is not yet confirmed to work". |
| https://github.com/danfromtico/switch-dev-legacy | — | — | — | 20.1 | — | Keeps Mesa 20.1 nvc0; no Vulkan. |

### A.3 How NVK reaches the GPU on Horizon (VM_BIND question)

- None of the ports emulate Linux nouveau's new uAPI (`VM_BIND`/`EXEC`) and none use devkitPro's `libdrm_nouveau`. danfromtico's backend replaces `nvkmd/nouveau` with `nvkmd/switch` and a `src/nouveau/horizon` runtime that is "a process-wide, reference-counted libnx service guard" wrapping NvMap IDs/handles, VA create/bind/unbind, channels with exec/submit/`wait_idle`, syncpoint-based fences, explicit cache `sync_to_gpu`/`sync_from_gpu` on 128-byte atoms, and `nvExit()` at shutdown (header: https://raw.githubusercontent.com/danfromtico/mesa-switch/main/src/nouveau/horizon/nouveau_horizon.h). D3fau4 states the same design goal explicitly (above). `nvk_physical_device.c` in the fork compiles `nvk_enumerate_physical_device()` only under `#ifdef __SWITCH__`, calling `nvkmd_try_create_pdev()` instead of the DRM path (https://raw.githubusercontent.com/danfromtico/mesa-switch/main/src/nouveau/vulkan/nvk_physical_device.c).
- Upstream status: NVK docs list Kepler→Ada + consumer Blackwell on Linux ≥ 6.6; Tegra appears only as `NVK_DEBUG=coherent` ("only applies to Tegra devices") (https://docs.mesa3d.org/drivers/nvk.html). mesa-dev, 2026-07-24 "Tegra and Vulkan 1.3 Inquiry" (Gabriele Melluso) and reply 2026-07-27 by Faith Ekstrand: Aaron Kling "has been working on this precisely for the switch", "most of it does work", the gap is "some kernel work to fix issues that Vulkan seems to hammer where GL doesn't"; tracker gitlab mesa/mesa#14105 (http://www.mail-archive.com/mesa-dev@lists.freedesktop.org/msg225193.html, …/msg225194.html; gitlab issue page is bot-blocked, not read). That is the Linux/nouveau-kernel path, not Horizon.

### A.4 Maturity verdict (inference)

Active (daily commits in both main lineages as of 2026-10-13), but 1–4 months old, two incompatible lineages with different licences (MIT vs GPL-2.0-or-later static-link obligations), no CTS results, self-reported 79/84 Sascha Willems samples, "not conformant" by NVK's own gate, GM20B-specific hangs still being fixed in Sept/Oct 2026, GL coexistence disputed (#439 says Mesa-26 GL black-screens; mesa-switch README says nvc0/Zink GL works), and no distribution through devkitPro pacman. Toolchain burden is real: Rust/NAK cross-build, MSYS2 for the unified SDK, or the autorunhq Docker image.

## 3. Question B — Feature level vs Godot 4.7's RenderingDevice minimums

### B.1 What NVK reports on GM20B (verified in fork sources)

`nvk_get_vk_version()` in https://raw.githubusercontent.com/danfromtico/mesa-switch/main/src/nouveau/vulkan/nvk_physical_device.c (same logic in nxvk `switch` branch):

1. version override if set;
2. `if (info->cls_eng3d < MAXWELL_A)` → 1.2 ("Vulkan 1.3 requires vulkanMemoryModel which isn't supported by Kepler");
3. `if (!nvk_supports_host_image_copy(info))` → 1.3; `nvk_supports_host_image_copy` = `cls_eng3d >= TURING_A || info->type == NV_DEVICE_TYPE_SOC`;
4. else 1.4.

GM20B is Maxwell and a SoC → **reports Vulkan 1.4** in current source. Builds from Aug–Sep 2026 reported **1.3** (Godot log "Vulkan 1.3.354", GBAtemp 2026-08-26; #439 "Vulkan 1.3"); nxvk added "Support VK_EXT_host_image_copy on SoCs" on 2026-09-18. Inference: the 1.3→1.4 bump is that commit.

Gating: `nvk_is_conformant()` returns false for `info->type != NV_DEVICE_TYPE_DIS` ("Tegra is not currently supported"); the `NVK_I_WANT_A_BROKEN_VULKAN_DRIVER` check is compiled out under `#ifndef __SWITCH__` in danfromtico's fork (so no env var needed there), whereas nxvk's README says device creation fails without it; Ozuba sets it regardless.

Feature gates relevant to Maxwell (same file): sparse* = `>= MAXWELL_B` but disabled under `__SWITCH__` ("no ctx_bind implementation"); `samplerFilterMinmax`, `shaderOutputViewportIndex/Layer` `>= MAXWELL_B`; `vulkanMemoryModel`, `descriptorBuffer`, `shaderStorageImageReadWithoutFormat` `>= MAXWELL_A`; `shaderFloat16` needs `sm >= 70` (no on GM20B, sm53); `drawIndirectCount`, mesh/task, FSR, cooperative matrix need Turing (no). `depthBounds`, `shaderInt16`, descriptor-indexing features are set true unconditionally. `__SWITCH__` also disables present-wait/present-id/present-timing and `NVX_binary_import`. nxvk README: `textureCompressionASTC_LDR` true; no fd/dma-buf external memory on either fork. Inference: GM20B's 3D class is Maxwell-B (GM20x), so the MAXWELL_B gates pass; I did not verify the class constant the Horizon backend reports.

### B.2 What Godot 4.7.2 actually requires (verified in this worktree)

`drivers/vulkan/rendering_context_driver_vulkan.cpp`:
- l.416: falls back to `VK_API_VERSION_1_0` if `vkEnumerateInstanceVersion` is absent; app API version is 1.0 or 1.2 (l.698).
- Required instance extensions: `VK_KHR_surface` (l.431) and the platform surface extension if the platform provides one (l.432–433; Ozuba returns `VK_NN_VI_SURFACE_EXTENSION_NAME`). `VK_KHR_get_physical_device_properties2`, `VK_EXT_swapchain_colorspace`, debug extensions are optional (l.441–460).

`drivers/vulkan/rendering_device_driver_vulkan.cpp`:
- Only required device extension: `VK_KHR_swapchain` (l.565). Everything else (multiview, FSR, create_renderpass2, float16/int8, 16-bit storage, image_format_list, maintenance2, pipeline cache control, subgroup size control, BDA, memory model, sync2, depth_stencil_resolve, ray tracing…) is optional (l.566–618). `VK_KHR_shader_non_semantic_info` is required only when SPIR-V debug info is enabled (l.594–596).
- Required device features (l.771–792): `imageCubeArray` and `independentBlend`; otherwise `ERR_CANT_CREATE`. All other features are opt-in-if-present (`VK_DEVICEFEATURE_ENABLE_IF`, l.827–873). `shaderSampledImageArrayNonUniformIndexing` is only asserted under `MACOS_ENABLED` (l.1015–1017).
- Docs: Forward+ and Mobile minimum is "Vulkan 1.0" on desktop and mobile SoCs (https://docs.godotengine.org/en/stable/about/system_requirements.html).

Compute: both RD renderers use compute lists regardless of method — `storage_rd/particles_storage.cpp` (l.1207, 1320, 1338, 1710), `storage_rd/mesh_storage.cpp` skeletons (l.1166), `effects/copy_effects.cpp` (11 compute lists), plus `canvas_sdf.glsl`, `sort.glsl`, etc. (`grep -l '#[compute]'` lists 50+ shaders). Mobile differs only in that `renderer_scene_render_rd.cpp` l.1848–1870 switches copy/gaussian-blur (and octmap if the A2B10G10R10 storage format is unsupported) to raster variants when `_render_buffers_can_be_storage()` is false, and `forward_mobile/render_forward_mobile.h` stubs out SDFGI (`sdfgi_update` no-op, l.643–644). SDFGI, VoxelGI, SSAO/SSIL, SSR, volumetric fog, TAA, FSR2 are Forward+-only compute passes (`shaders/environment/*`, `shaders/effects/*`). Compute shaders are Vulkan 1.0 core, so any NVK build satisfies this.

### B.3 Verdict (inference from B.1 + B.2 + C)

Vulkan 1.3/1.4 + `VK_KHR_surface` + `VK_NN_vi_surface` + `VK_KHR_swapchain` + `imageCubeArray`/`independentBlend` (core Maxwell capabilities; NVK enables them for all generations) exceed Godot's floor. The empirical confirmation is the Godot 4.5.1 "Forward Mobile" init on "NVK GM20B" (C.2). Expected gaps are performance/robustness, not capability: `shaderFloat16` absent (Godot uses it optionally), no present-id/wait, NAK first-boot compile cost, GM20B hang fixes still landing.

## 4. Question C — Evidence of Godot 4.x Forward+/Mobile on Switch homebrew via Vulkan

1. **Ozuba/godot `switch-platform`** (native port, Godot 4.7-rc). `platform/switch/README.md` (https://raw.githubusercontent.com/Ozuba/godot/switch-platform/platform/switch/README.md): "Forward+ and Mobile use RenderingDevice on Vulkan over NVK, presenting through a VK_NN_vi_surface WSI with a zero-copy block-linear swapchain on the libnx nwindow. Mobile is the method sized for the Tegra X1; Forward+ runs but is heavier"; Compatibility runs GLES3 via Zink by default; NVK "needs the full application memory pool to initialize its GPU channel, so launch with title takeover"; "The NVK WSI is FIFO-only"; first boot compiles all pipelines through NAK. `rendering_context_driver_vulkan_switch.cpp` is a 20-line `VK_NN_vi_surface` context (`vkCreateViSurfaceNN` on `wpd->window`, validation layers off: "Loaderless static ICD; there are no Vulkan layers on Horizon"). `godot_switch.cpp` `setup_nvk_env()` defaults `MESA_SWITCH_GL_DRIVER=zink`, sets `NVK_I_WANT_A_BROKEN_VULKAN_DRIVER`, `RAYON_NUM_THREADS=1`/`MESA_SHADER_COMPILER_THREADS=1` (Rust `std::thread` TLS issue on Horizon), `NVK_DEBUG` no-compression ("nothing on Horizon manages comptags"), shader cache on SD capped at 256M. Branch dates (https://github.com/Ozuba/godot/branches/all): `switch-platform` 2026-09-12, `switch-platform-nvk` 2026-08-27 (`2b77d2ec93` "Testing with new nvk compilation"; `ded7c60559` "Initial proof of concept vulkan port" 2026-06-12), `master` 2026-06-10. Self-reported only: 0 stars/forks, no screenshots or videos found.
2. **ChanseyIsTheBest/sts2_nx** (Slay the Spire 2, GBAtemp thread 2026-08-26/27, https://gbatemp.net/threads/slay-the-spire-2-switch-port.683997/, repo https://github.com/ChanseyIsTheBest/sts2_nx created 2026-08-26, 8 stars). A user's `godot.log` shows "Godot Engine v4.5.1.stable.mono.custom_build", renderer **"Forward Mobile"**, **"Vulkan 1.3.354"**, device **"NVIDIA Tegra X1 (GM20B) (NVK GM20B)"**. The README says it loads the game's own `libgodot_android.so` behind a fake JNI/JavaVM with "a Vulkan context on the console's own window", "Vulkan runs on Mesa's NVK with a Switch backend (`nvkmd-switch`, `wsi_switch`)"; credits "dantico and Nagaa for the Vulkan driver". Reported: enemy-attack visual issues "due to Mesa driver issues", UI diagonal line, cutscene glitches, flicker, stutter, audio not playing; first boot converts textures to ASTC (20–30 min). This is the strongest third-party evidence that Godot's Vulkan RD driver initialises and renders on NVK/GM20B — via the Android binary, not a native `platform/switch`.
3. **Thorhax/Gizmoduck-NX-Modern** (from local notes): same wrapper technique but GLES3 via Mesa, not Vulkan.
4. No videos, no Forward+ demonstrations by anyone other than Ozuba's README sentence, no Godot issue-tracker entries. Searches run: "Ozuba godot switch-platform NVK Vulkan", "Godot 4 Vulkan Nintendo Switch homebrew Forward+ Mobile renderer running Atmosphere", "Godot 4 Switch homebrew NVK Vulkan video OR youtube OR gbatemp 'Forward Mobile' OR 'Forward+' Tegra 2026", "NVK nouveau Vulkan Nintendo Switch homebrew libnx mesa", plus the GBAtemp/Flycast threads above. Wololo's 2023 article (https://wololo.net/2023/09/18/the-state-of-godot-engine-on-homebrew-consoles/) predates all of this and said no homebrew Vulkan existed.

## 5. Question D — How a Vulkan swapchain presents on libnx

Verified in https://raw.githubusercontent.com/danfromtico/mesa-switch/main/src/vulkan/wsi/wsi_common_switch.c and `nvk_switch_wsi.h` (fetched 2026-10-13):

- Surface: `VK_NN_vi_surface`; the `window` field is a libnx `NWindow*` (Ozuba passes `nwindowGetDefault()` via `WindowPlatformData`). nxvk additionally lists `VK_EXT_headless_surface`.
- Swapchain: `minImageCount = maxImageCount = 3`, `num_images` fixed at 3 (NWindow "permits one dequeued slot"). Images are `VK_IMAGE_TILING_OPTIMAL` dedicated allocations, block-linear with compression disabled ("Horizon scanout cannot decode GPU compression tags"), registered with the Horizon compositor as `NvGraphicBuffer` (`NvKind_Generic_16BX2`, `NvLayout_BlockLinear`) via `nwindowConfigureBuffer`; `WSI_SWAPCHAIN_NO_BLIT` — **zero-copy scanout, no blit**.
- Present modes: `FIFO` (swap interval 1) and `IMMEDIATE` (swap interval 0) via `nwindowSetSwapInterval`; **no MAILBOX**. (Ozuba's README "FIFO-only" describes an earlier build — inference.)
- Formats: `R8G8B8A8_UNORM` and `B8G8R8A8_UNORM`, `SRGB_NONLINEAR` colour space only. No present scaling, no protected images, IDENTITY transform, OPAQUE/PRE_MULTIPLIED alpha.
- Acquire: `bqDequeueBuffer` (+ `eventWait` on the release event), release `NvMultiFence` imported into the app's semaphore/fence (`nvk_switch_semaphore_import_nvmultifence`); `VK_NOT_READY` on zero timeout.
- Present: `nwindowQueueBuffer` with the GPU `NvMultiFence` from the signalled `VkFence` (`nvk_switch_fence_peek_nvmultifence`), else a host `WaitForFences` first.
- Resize/dock: no in-place resize; dimensions from `nwindowGetDimensions`; `create_swapchain` calls `nwindowSetDimensions`; stale slots return `VK_ERROR_OUT_OF_DATE_KHR`, and old swapchain buffers are released via `oldSwapchain`. Godot's `rendering_device_driver_vulkan.cpp` already recreates on `OUT_OF_DATE` (l.3333, 3996–4033), so handheld↔docked works through the normal Godot path if the platform layer updates the NWindow dimensions first (Ozuba does this with `appletHook` for the EGL path).
- Memory/applet: Ozuba's README says NVK needs the full application pool (title takeover) to create its channel; `nouveau_horizon.h` mentions reclaiming the heap for the homebrew loader at shutdown.

So: **a real `VK_KHR_surface`/`VK_KHR_swapchain` for an NWindow exists; no offscreen + manual-present path is required.** The headless surface could be used for an offscreen path, but nothing in Godot needs it.

## 6. Implications for this fork (inference)

- Nothing changes for the Compatibility-only plan in `.scratch/switch-port`: official portlibs remain GL-only.
- If Vulkan is wanted later, the pieces exist and are small on the Godot side (Ozuba's ~20-line context driver + `detect.py` link list), but they pin the port to a non-devkitPro Mesa 26 SDK (danfromtico MIT lineage is the licence-safe one; nxvk's GPL additions would propagate to the NRO), need the Rust/NAK cross-toolchain or the `autorunhq/switch-dev` Docker image, title-takeover launch, and a driver whose GM20B hang fixes are weeks old. Mobile is the realistic method (Ozuba; STS2 evidence), Forward+ is unproven beyond one sentence.

## 7. Source list

Local (read-only): `git show HEAD:.scratch/switch-port/research/03-prior-art.md`, `…/05-gpu-stack.md`; `drivers/vulkan/rendering_context_driver_vulkan.cpp`, `drivers/vulkan/rendering_device_driver_vulkan.cpp`, `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp`, `servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h`, `servers/rendering/renderer_rd/storage_rd/{particles_storage,mesh_storage}.cpp`, `servers/rendering/renderer_rd/effects/copy_effects.cpp`, `servers/rendering/renderer_rd/shaders/**` (all at `4.7.2-stable-5-g0c76a51e10`).

Web (all fetched 2026-10-13):
- https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mesa/PKGBUILD ; GitHub API commits for `switch/mesa`
- https://github.com/devkitPro/mesa/branches
- https://github.com/devkitPro/pacman-packages/issues/439 (moved; content from search snippets)
- https://github.com/danfromtico/mesa-switch ; `/pull/1` ; raw `src/nouveau/vulkan/nvk_physical_device.c`, `src/vulkan/wsi/wsi_common_switch.c`, `src/nouveau/vulkan/nvk_switch_wsi.h`, `src/nouveau/horizon/nouveau_horizon.h` ; GitHub API repo/commits/contents
- https://github.com/NaGaa95/mesa-switch ; https://github.com/etonedemid/mesa-switch ; https://github.com/autorunhq/switch-dev ; https://github.com/danfromtico/switch-dev-legacy
- https://github.com/PalindromicBreadLoaf/nxvk ; https://github.com/StevensND/nxvk/blob/switch/switch/README.md ; raw nxvk `src/nouveau/vulkan/nvk_physical_device.c`
- https://github.com/HayatoG/switch-nvk ; https://github.com/eriqueh2690-dev/switch-nvk9852 ; https://github.com/D3fau4/mesa-nvk-horizon
- https://github.com/Ozuba/godot ; `/branches/all` ; raw `platform/switch/README.md`, `platform/switch/rendering_context_driver_vulkan_switch.cpp`, `platform/switch/godot_switch.cpp` ; GitHub API commits on `switch-platform-nvk`
- https://gbatemp.net/threads/slay-the-spire-2-switch-port.683997/ ; https://github.com/ChanseyIsTheBest/sts2_nx
- https://gbatemp.net/threads/so-dan-didnt-want-to-reveal-his-vulkan-so-i-made-mine.682214/
- https://github.com/flyinghead/flycast/issues/2409
- https://docs.mesa3d.org/drivers/nvk.html
- http://www.mail-archive.com/mesa-dev@lists.freedesktop.org/msg225193.html and msg225194.html (gitlab mesa/mesa#14105 not readable: Anubis bot wall)
- https://docs.godotengine.org/en/stable/about/system_requirements.html
- https://wololo.net/2023/09/18/the-state-of-godot-engine-on-homebrew-consoles/
