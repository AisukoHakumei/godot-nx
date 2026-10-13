# Feasibility: a Godot 4.7 `RenderingDeviceDriver` backend on deko3d

Date: 2026-10-13. Repo inspected: `/home/aisuko/gamedev/godot-nx` at `4.7.2-stable-5-g0c76a51e10` (branch `switch-4.7`). Read-only investigation.

Legend: **[V]** = verified from a cited file or URL. **[I]** = inference / estimate by the author of this report.

---

## 1. What a backend must implement (local, [V])

### 1.1 The three interfaces

| Interface | File | Pure virtuals | Notes |
|---|---|---|---|
| `RenderingDeviceDriver` (RDD) | `servers/rendering/rendering_device_driver.h` (960 lines) | **138** | 136 single-line `virtual … = 0;` plus 2 multi-line (`command_pipeline_barrier`, `render_pipeline_create`) |
| `RenderingContextDriver` | `servers/rendering/rendering_context_driver.h` (138 lines) | **25** | device enumeration, surface create/size/vsync, HDR-output knobs (8 of the 25 are HDR getters/setters), `driver_create()` |
| `RenderingShaderContainer` + `RenderingShaderContainerFormat` | `servers/rendering/rendering_shader_container.h` (314 lines, .cpp 1014 lines) | **6** (`_format`, `_format_version`, `_set_code_from_spirv`, `create_container`, `get_shader_language_version`, `get_shader_spirv_version`) plus ~15 optional `_from_bytes_*/_to_bytes_*` extra-data hooks | the per-driver shader blob format |

Also a long tail of non-pure virtuals with defaults (`shader_get_layout_hash`, `linear_uniform_set_pools_reset`, `uniform_sets_have_linear_pools`, …) and the `Features`/`Limit` enums in `rendering_device_commons.h` (45 `LIMIT_*` values; ~15 `SUPPORTS_*` features).

### 1.2 RDD method groups (from the section headers in `rendering_device_driver.h`)

| Group | Pure virtuals (approx.) | deko3d mapping [I] |
|---|---|---|
| Init / `initialize(device_index, frame_count)` | 1 | `dkDeviceCreate`, `dkQueueCreate` |
| **Buffers** (`buffer_create/free/map/unmap/persistent_map_advance/get_device_address/get_dynamic_offsets/set_texel_format`) | 9 | sub-allocate from `DkMemBlock`; device address = `DkGpuAddr` |
| **Textures** (`texture_create/_from_extension/_shared/_shared_from_slice/free/get_copyable_layout/get_data/get_usages_supported_by_format/can_make_shared_with_format`) | 11 | `DkImageLayout`+`DkImage` in `DkMemBlockFlags_Image` memory; views = `DkImageView` |
| **Samplers** | 3 | `DkSampler` → `DkSamplerDescriptor` |
| **Vertex formats** | 2 | `DkVtxAttribState`/`DkVtxBufferState` |
| **Barriers** (`command_pipeline_barrier` with memory/buffer/texture/AS barriers; image layouts) | 1 | `dkCmdBufBarrier(DkBarrier_*, DkInvalidateFlags_*)`; no layouts exist on deko3d |
| **Fences / Semaphores** | 5 | `DkFence` + `dkFenceWait`; semaphores ⇒ `dkQueueWaitFence/SignalFence` or no-op on a single queue |
| **Command queues / pools / buffers** (`command_queue_family_get/create/execute_and_present/free`, `command_pool_*`, `command_buffer_create/begin/begin_secondary/end/execute_secondary`) | 12 | `DkCmdBuf` + `DkCmdList`; secondary ⇒ `dkCmdBufCallList` |
| **Swapchain** (`swap_chain_create/resize/acquire_framebuffer/get_render_pass/get_format/get_color_space/get_hdr_output_supported/free`) | 8 | `DkSwapchain` on `nwindowGetDefault()`, `dkQueueAcquireImage` / `dkQueuePresentImage` |
| **Framebuffers / Render passes** (`framebuffer_create/free`, `render_pass_create/free`, `command_begin/end_render_pass`, `next_subpass`, viewport/scissor/clear_attachments) | 11 | **no render-pass object in deko3d**; emulate as Metal/D3D12 do: resolve subpasses to `dkCmdBufBindRenderTargets` + clears/discards at begin |
| **Shaders** (`shader_create_from_container`, `shader_free`, `shader_destroy_modules`) | 3 | load DKSH into `DkMemBlockFlags_Code` memory, `dkShaderInitialize` |
| **Uniform sets** (`uniform_set_create/free`, `uniform_sets_get_dynamic_offsets`, `command_uniform_set_prepare_for_use`) | 4 | descriptor tables (`DkImageDescriptor`/`DkSamplerDescriptor`, 32 B each) + `DkResHandle`; UBO/SSBO via `dkCmdBufBindUniformBuffers/StorageBuffers` |
| **Transfer** (clear/copy buffer, copy texture, resolve, clear color/depth, buffer↔texture) | 8 | `dkCmdBufCopyBuffer/CopyImage/CopyBufferToImage/CopyImageToBuffer`, `dkCmdBufClearColor/DepthStencil` |
| **Pipelines** (`render_pipeline_create`, `compute_pipeline_create`, `pipeline_free`, pipeline cache ×4, `command_bind_push_constants`) | 8 | **no PSO in deko3d**: a "pipeline" becomes a struct of `DkRasterizerState/DkColorState/DkDepthStencilState/DkBlendState/…` bound at `command_bind_*_pipeline`; push constants ⇒ `dkCmdBufPushConstants` into a reserved UBO slot |
| **Render commands** (bind pipeline/uniform sets, draw ×6 incl. indirect & indirect-count, bind vertex/index buffers, blend constants, line width) | 11 | direct; **no multidraw / indirect-count** (README "not yet implemented: Multidraw") ⇒ loop on CPU |
| **Compute commands** (bind pipeline/uniform sets, dispatch, dispatch_indirect) | 5 | `dkCmdBufDispatchCompute(Indirect)`; queue needs `DkQueueFlags_Compute` |
| **Raytracing** (blas/tlas create, AS instance write, RT pipeline create/free/get handles, build blas/tlas, bind RT pipeline/uniform set, trace rays) | 13 | stub with `ERR_FAIL` exactly as Metal does (`drivers/metal/rendering_device_driver_metal.cpp:2275-2329`) |
| **Timestamp queries** (pool create/free/get_results/result_to_time, reset, write) | 6 | `dkCmdBufReportCounter(DkCounter_Timestamp, addr)` + `dkTimestampToNs` |
| **Labels / breadcrumbs / segments** | 5 | no-ops |
| **Misc** (`set_object_name`, `get_resource_native_handle`, memory stats, `limit_get`, `has_feature`, multiview/VRS/FDM caps, api name/version, pipeline cache uuid, `get_capabilities`, `get_shader_container_format`) | 15 | `DeviceFamily` enum has only OPENGL/VULKAN/DIRECTX/METAL; add `DEVICE_DEKO3D` or report UNKNOWN |

### 1.3 How shaders enter the driver [V]

1. Engine GLSL (`servers/rendering/renderer_rd/shaders/*.glsl`, 102 files, `#version 450` Vulkan-style with `set`/`binding`, `push_constant`, separate `texture2D`+`sampler`) is compiled **at runtime** by glslang: `ShaderRD::_compile_variant` → `RD::shader_compile_spirv_from_source` (`shader_rd.cpp:1167`, via `modules/glslang`).
2. SPIR-V → driver blob: `RenderingDevice::shader_compile_binary_from_spirv` (`rendering_device.cpp:4152`) does `driver->get_shader_container_format().create_container()` then `container->set_code_from_spirv(name, spirv)`. The base class runs **SPIRV-Reflect** to fill generic reflection (sets, bindings, push-constant size, spec constants), then calls the driver's `_set_code_from_spirv`:
   - Vulkan: stores SPIR-V (optionally SMOL-V compressed) — `drivers/vulkan/rendering_shader_container_vulkan.cpp` (117 lines).
   - D3D12: SPIR-V → DXIL through Mesa NIR (`godot_nir`, `spirv_to_dxil`) — 1022 lines.
   - Metal: SPIR-V → MSL through **SPIRV-Cross `CompilerMSL`**, then optionally **shells out to `/usr/bin/xcrun … metal`** to produce a `.metallib` (`drivers/metal/rendering_shader_container_metal.cpp:64,184,206,324-392,725`) — 877 lines. This is the precedent for "external compiler process at export time".
3. Blob → GPU object: `RD::shader_create_from_bytecode_with_samplers` (`rendering_device.cpp:4172`) → `container->from_bytes()` → `driver->shader_create_from_container(container, immutable_samplers)`.
4. Specialization constants are **not** baked into the blob: they arrive at `render_pipeline_create` / `compute_pipeline_create` as `VectorView<PipelineSpecializationConstant>`. Vulkan passes `VkSpecializationInfo`; Metal builds `MTLFunctionConstantValues` (`rendering_device_driver_metal.cpp:1757-1804`); D3D12 **binary-patches the DXIL** and re-signs it (`RenderingDXIL::patch_specialization_constant`, `rendering_device_driver_d3d12.cpp:3229-3245`).

### 1.4 Shader baker (offline compile at export) [V]

- `editor/export/shader_baker_export_plugin.cpp`: active when the export preset has feature `shader_baker` and an RD renderer is running (`:57-58`). It picks a `ShaderBakerExportPluginPlatform` whose `matches_driver(rendering/rendering_device/driver)` is true (`:61-68`), bakes every `ShaderRD` embedded version **and every embedded material shader** (`:94-170`, `shaders_embedded_set_get`, `MaterialStorage::shader_embedded_set_get`) into `shader_baker/<os>/<driver>/…` using `ShaderRD::version_get_cache_file_relative_path` (`:385`).
- Runtime load: `ShaderRD::_load_from_cache` (`shader_rd.cpp:614-674`) looks in the user cache dir and then the **res:// cache dir** and calls `shader_create_from_bytecode_with_samplers`, so no glslang is needed for anything that was baked.
- Registered platforms: Vulkan, D3D12, Metal (`editor/editor_node.cpp:9503-9519`, under `#ifdef VULKAN_ENABLED/D3D12_ENABLED/METAL_ENABLED`); platform headers in `editor/shader/shader_baker/`. Only macOS and Android export presets currently expose `shader_baker/enabled` (`platform/macos/export/export_plugin.cpp:498`, `platform/android/export/export_plugin.cpp:2254`). The Metal baker refuses non-macOS/iOS targets (`shader_baker_export_plugin_platform_metal.cpp:51`).
- **Conclusion:** yes, Godot can compile shaders offline at export into a driver-specific format, and the Metal path already proves that a host-side external compiler can be invoked from the baker. A `ShaderBakerExportPluginPlatformDeko3D` + `RenderingShaderContainerDeko3D` would slot in without touching the generic machinery. What is **not** baked: shaders created at runtime from GDScript (`Shader.code = …`), and non-embedded materials loaded from outside the PCK.

### 1.5 What the Mobile renderer actually needs from the driver [V]

- Vulkan hard-requires only `imageCubeArray` and `independentBlend` (`rendering_device_driver_vulkan.cpp:771-790`); everything else is opt-in (`VK_DEVICEFEATURE_ENABLE_IF`) and `shaderSampledImageArrayNonUniformIndexing` is checked only when the relevant path is used (`:1016`).
- **Compute is mandatory** even for Mobile: `compute_list_begin`/`compute_pipeline_create` are used by shared storage and effects (`storage_rd/mesh_storage.cpp` skinning/blend shapes, `particles_storage.cpp`, `texture_storage.cpp`, `effects/copy_effects.cpp`, `luminance.cpp`, `resolve.cpp`, `bokeh_dof.cpp`, `taa.cpp`, `fsr*.cpp`, `environment/fog.cpp`, `gi.cpp`, …). deko3d has compute (`DkStage_Compute`, `dkCmdBufDispatchCompute`) so this is fine.
- **Multiview** (`GL_EXT_multiview`, 11 shader files) is gated behind `SUPPORTS_MULTIVIEW` / `MultiviewCapabilities.is_supported` and only used for XR; a deko3d driver can report `false`.
- **Subgroup ops** (`GL_KHR_shader_subgroup_*`) appear only in `forward_clustered/*` , `cluster_render.glsl`, `volumetric_fog_process.glsl` — Clustered-only. Mobile is unaffected.
- Optional features queried by `has_feature`: `SUPPORTS_HALF_FLOAT` (`GL_EXT_shader_16bit_storage`/`float16`, 2 uses), `SUPPORTS_IMAGE_ATOMIC_32_BIT`, `SUPPORTS_VULKAN_MEMORY_MODEL`, `SUPPORTS_FRAMEBUFFER_DEPTH_RESOLVE`, `SUPPORTS_POINT_SIZE`, `SUPPORTS_ATTACHMENT_VRS`, `SUPPORTS_METALFX_*`, `SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS`. All may be reported `false`.
- Render passes with **subpasses + `subpassLoad`** are used by Mobile (`effects/tonemap_mobile.glsl:49,734`, `input_attachment_index`). Metal already emulates subpasses (`rendering_device_driver_metal.cpp:1598-1604`), so there is precedent; on deko3d an input attachment degrades to a texture read after a `DkBarrier_Fragments`.
- **Specialization constants**: Mobile scene shader uses 4 (`scene_forward_mobile_inc.glsl:58-60,224`: `pso_sc_packed_0/1` (uint bitfields), `pso_sc_packed_2` (float), `sc_emulate_point_size` (bool)); 32 declarations across all RD shaders. Push constants are used in 68 of 102 shader files.
- Shader stages used by RD renderers: vertex, fragment, compute only (tessellation/geometry/RT enums exist but Mobile/Clustered don't use them).
- Mobile `scene_forward_mobile_inc.glsl` declares 43 `layout(set …)` bindings; no descriptor arrays (`sampler2D x[N]`) exist in any RD shader (grep returned none).

### 1.6 Size of the existing backends [V] (`wc -l`, 4.7.2)

| Driver | Device driver | Context driver | Shader container | Other | **Total** |
|---|---|---|---|---|---|
| `drivers/vulkan/` | 7,508 + 833 (.h) | 1,094 + 226 | 117 + 68 | 219 | **10,065** |
| `drivers/d3d12/` | 6,417 + 965 | 392 + 127 | 1,022 + 185 | 731 (NIR bridge, DXIL hash, …) | **9,839** |
| `drivers/metal/` | 2,917 + 566 (+ 374/115 metal3 split) | 500 + 140 | 877 + 238 | 6,225 (`metal3_objects` 2,395, `metal_objects_shared` 1,990, `pixel_formats` 1,471, device props/profile ~700) | **12,952** |

Metal's README credits MoltenVK; the "smallest" RD backend is still ~10k lines, of which the device driver proper is 3–7.5k.

---

## 2. deko3d capabilities (web, [V] unless noted)

Sources: README <https://github.com/devkitPro/deko3d> (repo created 2019-02-12, last push 2026-02-18, 0.5.0 released 2023-11-26, zlib licence); Primer <https://github.com/devkitPro/deko3d/blob/master/Primer.md>; header <https://raw.githubusercontent.com/devkitPro/deko3d/master/include/deko3d.h>; examples <https://github.com/switchbrew/switch-examples/tree/master/graphics/deko3d/deko_examples>.

| Topic | Finding |
|---|---|
| Nature | Explicit Vulkan-style API (devices, queues, command buffers), plain C + Vulkan-Hpp-style C++ wrapper, debug (`-ldeko3dd`, validation) and release (`-ldeko3d`, aborts silently on error) builds. Reverse-engineered from NVN; "~80% of common hardware features". |
| Shader stages | `DkStage_Vertex, TessCtrl, TessEval, Geometry, Fragment, Compute` (6). No mesh/RT. |
| Compute | Yes: `dkCmdBufDispatchCompute`, `dkCmdBufDispatchComputeIndirect`, queue flag `DkQueueFlags_Compute`, `maxConcurrentComputeJobs`. "Variable group size" unimplemented (irrelevant to Godot). |
| Pipeline objects | **None.** State is bound piecemeal on the command buffer: `dkCmdBufBindShaders`, `BindRasterizerState`, `BindMultisampleState`, `BindColorState`, `BindDepthStencilState`, `BindBlendStates`, `BindColorWriteState`, plus dynamic `SetStencil/SetDepthBias/SetBlendConst/SetDepthBounds/SetSampleMask`. |
| Render passes | **None.** `dkCmdBufBindRenderTargets(colorTargets[], depthTarget)` with `DK_MAX_RENDER_TARGETS = 8`; `dkCmdBufClearColor/ClearDepthStencil`, `dkCmdBufDiscardColor/DiscardDepthStencil` (clears respect scissor). |
| Descriptors | Two GPU-memory tables bound by address: `dkCmdBufBindImageDescriptorSet(addr, count)` and `dkCmdBufBindSamplerDescriptorSet(addr, count)`; descriptors are 32-byte opaque structs (`DK_DECL_OPAQUE(ImageDescriptor, 4, 32)`), alignment 0x20. Shaders see `DkResHandle = imageIdx (20 bits) \| samplerIdx << 20 (12 bits)` built by `dkMakeTextureHandle`, bound per stage via `dkCmdBufBindTextures/BindImages`. Per-stage slot limits: **16 UBO (max 64 KiB, 256-B aligned), 16 SSBO, 32 textures (combined image+sampler handle), 8 storage images**; compute UBO slots 6-15 are emulated as SSBO. |
| Push constants | `dkCmdBufPushConstants(uboAddr, uboSize, offset, size, data)` — any UBO has push-constant semantics (Primer). |
| Buffers | No buffer objects; raw `DkGpuAddr` + size (`DkBufExtents`). Vertex: 16 buffers / 32 attributes. Index formats via `DkIdxFormat`. Indirect draw/dispatch supported; **multidraw / indirect-count not implemented** (README TODO). |
| Memory | `DkMemBlock` (4 KiB-aligned, each an NvMap handle; "allocate large blocks, sub-allocate"). Flags: CPU/GPU `Uncached/Cached/None`, `Code` (shader code segment, 4 GiB reserve, freed code mappings are never reused), `Image` (adds compressed mapping), `ZeroFillInit`. `dkMemBlockFlushCpuCache`. |
| Images | Block-linear (default) or pitch-linear; `DkImageType` 1D/2D/3D/1DArray/2DArray/2DMS/2DMSArray/Rect/Cubemap/CubemapArray/Buffer; `DkMsMode_1x/2x/4x/8x`; `DkImageFlags_HwCompression`, `UsageRender/LoadStore/Present/2DEngine/Video`. Known 0.5.0 bug: wrong tile height for BC images with 6–8 block rows (mip 1 overwrites level 0) — reported by SwitchWakerHD. |
| Formats | BC1–BC7 (+sRGB), BC4/5 unorm/snorm, BC6H sf/uf, **ASTC 4x4…12x12 (+sRGB)**, ETC2 R/RG/RGB/PTA/RGBA (+sRGB), depth `S8, Z16, Z24X8, ZF32, Z24S8, ZF32_X24S8`, `RGBA16_Float`, `RG11B10_Float`, `E5BGR9_Float`, sRGB only for 8-bit RGB(A)/BGR(A) (no R8/RG8 sRGB). `dkImageFormatGetFlags` for introspection (added 2024-09). |
| Barriers | `dkCmdBufBarrier(DkBarrier_{None,Tiles,Fragments,Primitives,Full}, DkInvalidateFlags_{Image,Shader,Descriptors,Zcull,L2Cache})`. No image layouts. deko3d inserts a full cache-invalidating barrier after every `dkQueueFlush`. |
| Sync | `DkFence` (opaque, address must stay stable), `dkCmdBufSignalFence/WaitFence`, `dkQueueSignalFence/WaitFence`, `dkFenceWait(timeout_ns)`, `dkFenceImport` (Host1x syncpoint, 2025-01 "import external fence" helper). `DkVariable` for GPU↔CPU 32-bit signalling. No "event" primitive (README TODO). |
| Queries | `dkCmdBufReportCounter(DkCounter_Timestamp / TimestampPipelineTop / SamplesPassed / …, addr)` writes 64-bit counter + timestamp; `dkTimestampToNs`. Flushes tiled cache as a side effect. |
| Tiled rendering | `dkCmdBufTiledCacheOp(Disable/Enable/Flush/…)`, `dkCmdBufSetTileSize` — Maxwell "tiled cache" (binning) is opt-in; SwitchWakerHD notes deko3d 0.5.0 never enables it by default. Not a TBDR; subpass-style on-chip attachments do not exist. |
| Presentation | `DkSwapchainMaker{device, nativeWindow (libnx `NWindow*`, usually `nwindowGetDefault()`), pImages, numImages}`; `dkQueueAcquireImage` → slot, `dkQueuePresentImage(slot)` (flushes queue). Images need `DkImageFlags_UsagePresent`. Swapchain must be drained (`dkQueueWaitIdle`) and recreated on handheld↔docked mode change (1280×720 / 1920×1080). |
| Clip space | Defaults `DepthZeroToOne`, `OriginUpperLeft`, but **clip-space Y points up**; negative viewport heights are rejected (use `DkSwizzle_NegativeY` or negate in shader). Godot's Vulkan path uses negative-viewport Y-flip, so this is a mapping detail. |
| Examples | `deko_examples` ships Example01–09: SimpleSetup, Triangle, Cube, TexturedCube, **Tessellation**, **Multisampling**, MeshLighting, **DeferredShading**, **SimpleCompute**. |
| Missing (README TODO) | multidraw, conditional rendering, transform feedback, sparse textures, fragment-shader interlock, passthrough GS, `gl_ViewportMask`, sample-position reads, zero-bandwidth custom clear colours, events. |

---

## 3. Shader toolchain: uam, DKSH, SPIR-V [V]

- **uam** (<https://github.com/devkitPro/uam>, created 2020-04-05, tags v1.0.0 2020-04-04 and **v1.1.0 2020-12-16**, last push 2022-03-12) is a PC tool "based on mesa 19.0.8 sources" (Mesa GLSL parser → TGSI → nouveau `nv50_ir` codegen with deko3d customisations such as Maxwell dual-issue scheduling). CLI: `uam -s {vert,tess_ctrl,tess_eval,geom,frag,comp} -o out.dksh in.glsl` (also `-r` raw SASS, `-t` TGSI). It is shipped as a devkitPro pacman package (host tool) — the Linux dkp-pacman repo is the normal way to get it (README of deko3d: "install `switch-dev` group").
- **Input: GLSL only, upstream.** The README never mentions SPIR-V; the Primer states shaders are "native SASS for Shader Model 5.3 … GLSL and SPIR-V are not accepted at runtime". Since it inherits Mesa 19's nouveau GLSL support for GM20x, GLSL 4.5/4.6 core features (SSBO, image load/store, atomics, compute shared memory, `ARB_gpu_shader5`) are expected to work **[I]**.
- **uam rules that matter for Godot** (README): every UBO/SSBO/sampler/image needs an explicit `layout(binding=N)`; no default (non-block) uniforms; no shader linking (separable programs always); no transform feedback; `pixel_center_integer` errors; non-constant integer div/mod decays to float with a warning (SwitchWakerHD/nfsmw hit real bugs here); 64-bit float div/sqrt approximated.
- **SPIR-V input exists only in an experimental fork:** gnaghi/uam (<https://github.com/gnaghi/uam>, forked from averne/libuam; created 2026-02-18, pushed 2026-10-10; commit 2026-02-26 "Add SPIR-V input support to CLI", 2026-03-10 "Add glslang frontend + SPIR-V compiler integration"). Its README says the SPIR-V path goes straight to NV50_IR but **texture sampling is stubbed, SSBO/image access unsupported, compute untested** — not usable for Godot today. The same author's SwitchGLES README describes the work as "mostly vibe coded". Treat as unverified.
- **libuam (on-device compiler) is real and used in production homebrew:** averne/libuam (2022) builds uam as `libuam.a` with `-Dbuild_mode=lib` (C API `uam_compile_dksh`…); SwitchWakerHD vendors uam 1.1.0 "compiled from source by CMake… linked into the app as a library", single non-reentrant worker thread, **~95 ms avg / 77 ms median per shader on the console** (1020 MHz); nfsmw-nx measured **58 ms per variant**, 702 variants in 41 s, needing "seven patches" to uam (mutexes, DKSH-to-memory, log callback, no `exit()/abort()`, deterministic scheduling, dual-issue off). The Dolphin port (`nezumiiruka`, `Source/Core/VideoBackends/Deko3D/UamBridge.cpp`, `DKShaderCompiler.cpp`) does the same: feeds `#version 450 core` GLSL to an in-process uam build ("uam is mesa 19.0's single-threaded standalone compiler"; "uam has no include support").
- **SPIR-V → DKSH via SPIRV-Cross → GLSL → uam is the proven path.** nfsmw-nx PR #27 (<https://github.com/StevensND/nfsmw-nx/pull/27>, 2026-10-10): HLSL → DXC SPIR-V → SPIRV-Cross (GLSL 460, non-Vulkan semantics) → uam, "702 distinct DKSH variants compiled with 0 failures", console output byte-identical to PC output. Required source changes on their side: no bindless heaps, explicit bindings, **spec constants turned into compile-time macros (SPIRV-Cross emits `#ifndef SPIRV_CROSS_CONSTANT_ID_n`)**, push constants removed (uam cannot parse buffer references; but SPIRV-Cross `--glsl-emit-push-constant-as-ubo` emits them as a UBO block — see `main.cpp` help text at <https://raw.githubusercontent.com/KhronosGroup/SPIRV-Cross/main/main.cpp>), combined samplers (`build_combined_image_samplers`, `--combined-samplers-inherit-bindings`), dummy sampler for `texelFetch` on separate images (`build_dummy_sampler_for_combined_images`, auto when not using Vulkan semantics).
- Godot already ships SPIRV-Cross in `thirdparty/` (used by the Metal container) and SPIRV-Reflect (used by the base container), so no new third-party dependency is needed for the host side.

---

## 4. Prior art (web)

| Project | What | Maturity / numbers | URL / date |
|---|---|---|---|
| **nezumiiruka** (Dolphin GC/Wii emulator port, formerly "porpoise") | Hand-written `VideoBackends/Deko3D` implementing Dolphin's abstract `AbstractGfx/AbstractTexture/AbstractShader/AbstractPipeline/StreamBuffer/StateTracker` — the closest analogue to an RDD backend | 43 files, **221 KB ≈ 6–7k lines [I]** (`DKStateTracker.cpp` 19 KB, `DKTexture.cpp` 22 KB, `DKPipeline.cpp` 13 KB, `DKCommandBufferManager.cpp` 11 KB, `DKShaderCompiler.cpp` 6 KB, `UamBridge.cpp` 4 KB); author says deko3d "more performant than Vulkan [NXVK] in every case"; on-device shader compile ("Prepare shaders" takes minutes) | <https://github.com/PalindromicBreadLoaf/nezumiiruka> (created 2026-07-21, pushed 2026-10-11) |
| **SwitchWakerHD** (Wind Waker HD static recomp) | `Api::Deko3D` backend behind an existing `render::Backend` function table; replaced the OpenGL (switch-mesa) renderer; libuam on device + offline DKSH cache built on a Mac | 22→29 fps in heaviest view, 1/10 CPU time per frame vs GL (per release notes); plan estimated **6–9 weeks** (P0 2–3 d, P1 3–5 d, P2 2–3 wk, P3 1–2 wk, P4 1–2 wk, P5 2–3 d); P0–P5 actually landed 2026-10-07/08; written by an AI coding agent (Claude Code) with human testing | <https://github.com/centollOS/SwitchWakerHD>, `docs/deko3d-plan.md` (created 2026-10-04) |
| **nfsmw-nx** PR #27 (NFS Most Wanted recomp) | Second renderer under a PM4 ring, replacing NVK/Mesa Vulkan; SPIRV-Cross→uam pipeline; devlog with concrete deko3d lessons | **~3 calendar days** from first shader work to tuned build (2026-10-08→10); 27.7 vs 26.2 fps avg, far fewer >100 ms hitches (no pipeline creation on the ring thread: NVK took 49 ms/pipeline), NRO 39.9 vs 63.0 MB | <https://github.com/StevensND/nfsmw-nx/pull/27>, `docs/HowToDeko3d.md`, `docs/deko3d-devlog.md` |
| **SwitchGLES** | OpenGL ES 2.0 + EGL 1.4 on deko3d, optional on-device GLSL compiler | dEQP-GLES2 99.2 % pass; ES 2.0 only (ES 3.0 claimed in title but not in feature list); "mostly vibe coded" | <https://github.com/gnaghi/SwitchGLES> (2026-02-05 → 2026-10-10) |
| **borealis / nanovg-deko3d / moonlight-switch / wiliwili** | UI-level 2D renderers on deko3d | mature for 2D; .nro shrinks 9.4→3.7 MB vs Mesa | <https://github.com/xfangfang/borealis>, <https://github.com/Adubbz/nanovg-deko3d>, <https://github.com/JigSawFr/PlayGuard/pull/12> |
| **vita2hos** | PS Vita compatibility layer; uses uam | — | <https://github.com/xerpi/vita2hos> |
| **Godot ports** | Stary2001/godot (and forks PuntoScene/godot-nx, bitingraphics, redthing1, …) are **Godot 3.5.x on GLES via `switch-mesa`/nouveau** (credits fincs for the nouveau port). No Godot 4 homebrew port and **no Godot-on-deko3d attempt found** in any search. | GameBrew entry: 3.5.1, updated 2023-03; HN thread 2024-01-23 has no Vulkan/deko3d discussion | <https://github.com/Stary2001/godot>, <https://www.gamebrew.org/wiki/Godot_Engine_Switch>, <https://news.ycombinator.com/item?id=39102002> |
| wgpu / bgfx / SDL3 GPU / ANGLE / The Forge / Diligent / LLGL / sokol | **No deko3d backend found for any of them** (three searches). bgfx's backend list has no deko3d; SDL on Switch homebrew goes through switch-mesa GL. | — | search results only |

Lessons the 2026 projects report (all [V] from their docs, consolidated):
- Write descriptors with `dkCmdBufPushData` so they stay ordered with commands; never CPU-write a descriptor the GPU may still read; recycle slots after the frame fence.
- `DkBarrier_Full` stalls host prefetch and `Fragments` does a WFI; per-surface read/write epoch tracking cut barriers 47→20 per frame.
- Z-cull is only reset when the depth target address changes — invalidate manually after copies/uploads into depth.
- Release `libdeko3d` aborts without a callback; bring up with `libdeko3dd` and poll `dkQueueIsInErrorState`.
- `dkCmdBufBindVtxBuffers` writes one word past its reservation; bind one buffer per call and leave a 256-B gap at the end of each command-memory chunk.
- Depth textures sample as (D,D,D,1); `DkImageSwizzle` = Vulkan value − 1; sRGB framebuffer register possibly never written in 0.5.0 (unconfirmed); copy-engine copies broke compressed depth.
- `std::thread::detach()` throws on libnx.
- GPU timestamps need a ×1.627 calibration factor.

---

## 5. Mapping Godot RD onto deko3d: the hard parts [I unless noted]

1. **No runtime shader compiler is the central constraint.** Two viable designs, both with precedent:
   - *Offline-only*: `RenderingShaderContainerDeko3D::_set_code_from_spirv` runs on the **Linux host inside the editor** at export (SPIRV-Reflect → SPIRV-Cross GLSL 4.60 with `--glsl-emit-push-constant-as-ubo`, combined samplers inheriting bindings, set/binding flattened to per-stage slots → spawn `uam` per stage/variant, exactly like Metal spawns `xcrun metal`). On device, `set_code_from_spirv` returns false and `shader_create_from_bytecode` only loads baked DKSH. Requires registering a `ShaderBakerExportPluginPlatformDeko3D` and adding `shader_baker/enabled` to the Switch export preset. Breaks any shader created from code at runtime.
   - *Hybrid*: additionally link a patched libuam (as SwitchWakerHD/nezumiiruka/nfsmw do) and compile cache misses on a worker thread at ~60–100 ms each. Needs the 7 uam patches nfsmw documents; `uam` is single-threaded and non-reentrant.
2. **Specialization constants.** Godot resolves them at pipeline creation; DKSH is final SASS. Options: (a) bake one DKSH per (shader, spec-value tuple) — the Mobile scene shader packs its flags into `pso_sc_packed_0/1` uint bitfields, so the tuple space is large and not statically enumerable → needs either runtime libuam or an export-time recording of the tuples actually used (nfsmw: "mask specialization values to the bits a shader reads" cut this to one compile per vertex shader); (b) rewrite spec constants into a small UBO at SPIRV-Cross time (uniform branching; costs ALU/registers, trivial for the Mobile shader's 4 constants); (c) D3D12-style binary patching of SASS immediates — not realistic without a Maxwell SASS encoder. (b) is the pragmatic default, (a)+libuam the performance path.
3. **Render passes / subpasses.** Follow the Metal driver's `MDRenderPass`/`MDSubpass` emulation: at `command_begin_render_pass` bind targets, apply LOAD clears via `dkCmdBufClearColor/DepthStencil` (set scissor to full target first), `dkCmdBufDiscard*` for DONT_CARE; `command_next_render_subpass` → `DkBarrier_Fragments` + rebind; `subpassLoad` → SPIRV-Cross can't emit framebuffer-fetch on desktop GLSL without `GL_EXT_shader_framebuffer_fetch`, so rewrite input attachments to `texelFetch` on a combined sampler of the previous attachment (needs the fragments barrier). Only `tonemap_mobile.glsl` uses it.
4. **Pipelines.** `render_pipeline_create` returns a heap struct holding pre-translated `Dk*State` blocks + shader pointers; `command_bind_render_pipeline` binds them, diffing against a per-command-buffer shadow (nfsmw/SwitchWakerHD both do "emit only changed state"). Pipeline cache methods become no-ops. `SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS` or the D3D12 "layout hash" trait can be chosen freely.
5. **Uniform sets.** Godot's `uniform_set_create` gets `BoundUniform{type, binding, ids}` for one set index. Driver keeps a global image-descriptor table and sampler-descriptor table (ring-allocated per frame, written via `dkCmdBufPushData`), and for each set computes: per-stage UBO slot list (`dkCmdBufBindUniformBuffers`), SSBO list, `DkResHandle` list for textures (`dkMakeTextureHandle(imgIdx, smpIdx)`), image handles for storage images. The flattening (set,binding) → per-stage slot must be identical in the SPIRV-Cross remap at bake time and in the driver at bind time; the container can store the mapping table as "extra data" (`_to_bytes_reflection_binding_uniform_extra_data`). Hard limits to validate against Godot's `LIMIT_*`: 16 UBO / 16 SSBO / 32 textures / 8 images per stage. The Mobile scene shader's 43 bindings span all types; whether a complex user material exceeds 32 textures+samplers in one stage needs checking, and user-material `push_constant`+material UBO+scene UBOs must fit in 16 UBO slots (compute: 6 native).
6. **Push constants** → one reserved UBO slot per stage filled by `dkCmdBufPushConstants` (64 KiB max; Godot's are ≤128 B).
7. **Memory** → a sub-allocator over a few large `DkMemBlock`s per flavour (GPU-only block-linear image heap, CPU-uncached upload ring, CPU-cached readback, code segment that is never freed/reused). `buffer_map/unmap/persistent_map_advance` are natural on CPU-visible blocks. `texture_get_data` must handle block-linear → pitch-linear via `dkCmdBufCopyImageToBuffer`.
8. **Barriers/layouts** → ignore `TextureLayout` entirely (set `API_TRAIT_HONORS_PIPELINE_BARRIERS` so RD keeps calling the barrier hook), translate stage masks to the coarsest `DkBarrier_*` that covers the hazard, and add `DkInvalidateFlags_Zcull` on depth writes via copies.
9. **Sync** → `FenceID` = `DkFence`; `SemaphoreID` can be a no-op with one queue (Godot creates one graphics queue family and presents from it); `command_queue_execute_and_present` = `dkQueueSubmitCommands` × N + `dkCmdBufSignalFence` + `dkQueuePresentImage`.
10. **Swapchain** → `swap_chain_create(surface)` builds N `DkImage`s with `UsagePresent|UsageRender` (RGBA8 or BGRA8 unorm, `DATA_FORMAT_R8G8B8A8_UNORM`) on the `NWindow` carried in `surface_create(p_platform_data)`; `swap_chain_acquire_framebuffer` = `dkQueueAcquireImage`; HDR = unsupported. Resize on `appletGetOperationMode` change.
11. **Features to report**: no multiview, no VRS/FDM, no RT, no buffer-device-address (deko3d has raw GPU addresses but uam can't parse `GL_EXT_buffer_reference` per nfsmw), no half-float storage unless uam proves `GL_NV_gpu_shader5`/`EXT_shader_16bit_storage` works, `SUPPORTS_IMAGE_ATOMIC_32_BIT` likely yes (Maxwell), framebuffer depth resolve via `dkCmdBufResolveDepthValues`/copy.
12. **Texture formats**: Godot's `DATA_FORMAT_*` ↔ `DkImageFormat_*` table (≈100 entries, like `drivers/metal/pixel_formats.cpp`); BC/ASTC/ETC2 all present, so the existing import pipeline (`texture_get_usages_supported_by_format`) works. Missing: 8-bit single/dual-channel sRGB, 64-bit-per-channel formats (irrelevant).

Things that get easier than Vulkan: no VMA, no descriptor pools/layouts, no pipeline-cache serialisation, no validation-layer plumbing, single device/queue family, fixed GPU so `limit_get` is a constant table.

---

## 6. Effort (D) — what others report, and an estimate [I]

- Reported: SwitchWakerHD planned **6–9 weeks** for a game-specific GX2→deko3d backend (landed in ~4 days with an AI agent on hardware, with follow-up bug rounds). nfsmw-nx: **3 calendar days** to a tuned renderer, but on top of an existing renderer abstraction and with ~702 fixed shader variants. Dolphin port: **~6–7k lines** for a backend behind an abstraction broadly equivalent in surface to RDD (textures, shaders, pipelines, state tracker, stream buffers, swapchain, perf queries, compute-free). None of them publishes person-hours.
- Comparable Godot work: the RDD backends are 10–13k lines each; the Metal one was written by one engineer (stuartcarnie) over roughly a year part-time including shader container and baker **[I: from public PR history, not re-verified here]**.
- **Estimate for Godot-RD-on-deko3d** (Mobile renderer only, single queue, no XR/RT/VRS):
  - `drivers/deko3d/`: context driver ~400, device driver ~4–6k, shader container (SPIRV-Cross remap + uam invocation + DKSH loader + binding tables) ~1–1.5k, format tables ~500, export-plugin platform ~150 → **~7–9k lines**.
  - Toolchain risk work that is not line-count: validating that uam (Mesa 19.0.8) compiles all 102 engine GLSL files after SPIRV-Cross round-trip (expect a handful of failures around integer div, `samplerless` fetch, 16-bit storage, dynamic UBO component indexing, `GL_EXT_control_flow_attributes`), designing the spec-constant strategy, and the uam patch set for on-device use.
  - Rough calendar: **first triangle through RD (clear + 2D canvas)** 2–4 weeks for someone who knows both RD and deko3d; **Mobile 3D scene rendering with compute effects** another 6–10 weeks; **polish** (hitches, barriers, docked/handheld, baker UX, user-shader edge cases) open-ended. Total on the order of **3–5 person-months** for an experienced graphics programmer; the 2026 agent-assisted ports suggest the calendar can compress substantially when hardware test cycles are fast, but Godot's surface (138 methods, 102 shaders, user materials, spec constants) is much wider than a fixed-shader-set game recomp.
- **Alternative to weigh against:** Godot 4's **Compatibility (GLES3) renderer on `switch-mesa`/nouveau**, the path every Godot 3 Switch port took (Stary2001). Zero driver work, runtime glslang-free (GL compiles GLSL itself), larger NRO (+5–6 MB) and the CPU/stutter overhead that every deko3d port cites as the reason to leave Mesa. deko3d-RD is the performance/size route; GLES3-on-Mesa is the time-to-first-frame route.

---

## 7. Open questions to verify on hardware / toolchain before committing

1. Does stock `uam` 1.1.0 accept SPIRV-Cross output for a representative Godot shader set (e.g. `canvas.glsl`, `scene_forward_mobile.glsl` fp32 variant, `tonemap_mobile.glsl`, `luminance.glsl`, `copy.glsl`, `blend_shape.glsl`, `particles.glsl`)? A 1-day host-only experiment answers this.
2. Per-stage slot budgets (16/16/32/8) against the worst Godot material shader; whether `LIMIT_MAX_TEXTURES_PER_SHADER_STAGE` etc. can simply be reported low and the material compiler honours them.
3. Whether deko3d's tiled-cache opt-in matters for Godot's full-screen effects (SwitchWakerHD says it's never enabled by default).
4. The sRGB render-target register concern reported by SwitchWakerHD (unconfirmed) — Godot renders in sRGB swapchains by default.
5. Spec-constant strategy (uniform branching vs variant explosion vs on-device libuam).
