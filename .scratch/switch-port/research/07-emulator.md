# 07 — Emulator for fast iteration (Switch homebrew, devkitPro mesa/EGL OpenGL)

Research date: 2026-10-13. Scope: which Nintendo Switch emulator to use for day-to-day iteration on a
Godot 4.7.2 NRO built with devkitA64 + libnx, rendering through devkitPro mesa (nouveau/nvc0 over
libdrm_nouveau -> libnx nvdrv) + EGL. The real modded Switch remains the ground truth.

Access caveats: `git.ryujinx.app` (Ryubing's Forgejo) refused connections from the research
environment, `git.citron-emu.org` does not resolve, GitHub REST was rate-limited and several
GitHub repos return HTTP 451. Where a claim rests on a mirror or a secondary source it is marked.

## Summary

- **Recommendation: Eden (stable v0.2.1, 2026-06-01, or a nightly), run natively on the Windows host, not inside WSL2.**
- Eden is the only candidate with (a) commits this week (2026-10-13), (b) Windows + Linux builds, (c) an NRO loader that honours the libnx homebrew ABI for argv (PR #4013, merged 2026-06-15), and (d) explicit fixes for Mesa-compiled (nv50_ir) shaders (PRs #4012/#4151, June–July 2026).
- Ryubing (Ryujinx fork) is in maintenance mode since Feb 2026 (last stable 1.3.3, 2025-10-11), has no argv path for NROs, and has open homebrew bugs (NRO cannot see the SD card; an unimplemented nvhost ioctl crashes the emulator). Keep it as a second opinion only.
- Citron (original) is gone; Citron Neo is Vulkan-only with the old non-functional NSO-style `-p`. Sudachi, Suyu, Torzu, yuzu-mirror are dead or Tor-only. Nothing new and non-yuzu/non-Ryujinx appeared in 2025–26.
- Neither Eden nor Ryubing emulates nxlink; both expose `svcOutputDebugString` in their log (Eden: `Debug.Emulated` info; Ryujinx: `KernelSvc` warning). An Eden nxlink-server PR (#4233) is open but unmerged.
- NROs run without prod.keys or firmware in both (the loader never touches the key manager), but all of it is DMCA-exposed: GitHub release mirrors for Eden/Kenji-NX were taken down 2026-02-12; fetch Eden only from git.eden-emu.dev.
- Biggest divergences from hardware: emulators run the NRO as an *application* (3.2 GiB pool, `nvdrv`, `appletOE`) whereas hbmenu on hardware runs it as a *library applet* (~500 MiB pool, `nvdrv:a`); Eden only passes a loader config when `-p` is non-empty; wall-clock-scaled CPU timing; host-driver shader recompilation; Eden says NVIDIA support is "limited" (RADV is first-class).
- WSL2/WSLg is not a viable emulator host: its OpenGL is capped at 3.3 (Eden needs 4.6) and its Vulkan (dzn) is non-conformant and not packaged on Ubuntu. Build in WSL2, run the Windows Eden build on `\\wsl.localhost\...` paths.

## Comparison table

| Emulator | Maintenance (as of 2026-10-13) | Platforms | Loads NRO | argv to NRO | stdout / nxlink | GPU backends | nvdrv path used by mesa/deko3d | Keys/firmware for NRO | Distribution / legal |
|---|---|---|---|---|---|---|---|---|---|
| **Eden** (git.eden-emu.dev/eden-emu/eden) | Active. Latest commit 2026-10-13 ("[hle] constexpr handler dispatch maps…", #4480); stable v0.2.1 2026-06-01; nightlies daily (eden-ci/nightly, latest 2026-10-13) [E1][E2][E3] | Windows (MSVC amd64, amd64-v3, Zen4, arm64 zips), Linux (AppImage amd64/legacy/Steam Deck/Zen4/aarch64), Android, macOS (experimental), FreeBSD (rc builds) [E2] | Yes: `src/core/loader/nro.cpp` parses NRO0 + ASET (icon/NACP/RomFS), 39-bit AS, `ProgramMetadata::GetDefault()` [E4] | **Yes**, via the libnx homebrew ABI: PR #4013 writes `{MainThreadHandle, AppletType=Application, Argv, EndOfList}` + `"homebrew " + program_args` at the data-segment tail; CLI `--program/-p`, Qt/Android "Debug" setting `program_args` [E5][E6] | `svcOutputDebugString` -> `LOG_INFO(Debug_Emulated, …)` (console + log file) [E7]. nxlink: **not merged** — draft PR #4233 adds `_NXLINK_` marker handling, NextLoad, an "nxlink log server mode" (port 28771) [E8] | `RendererBackend {OpenGL_GLSL, Vulkan, Null, OpenGL_GLASM, OpenGL_SPIRV}`; requires Vulkan 1.1 or OpenGL 4.6; OpenGL dropped on Android 2026-09-02 [E9][E10][E11] | nvdrv/nvdrv:a/s/t registered; nvhost-gpu SubmitGPFIFO + KickoffPb, AllocGPFIFOEx2, AllocObjCtx, ZCullBind, SetErrorNotifier (STUBBED); nvhost-ctrl only Event* ioctls (SyncptWait 0x16 unimplemented — unused by libnx); nvhost-as-gpu InitializeEx/AllocSpace/MapBufferEx/GetVARegions; nvmap full. Mesa-specific fixes #4012 (nv50_ir `@PT EXIT` terminator, legacy-attribute IPA), #4151 (gated to non-proprietary driver), #4061 (ETC2), #3983 (fence id -1), #4264 (acc:e stubs) [E12]–[E17] | Not required (nro.cpp never references the key manager) [E4]; third-party RetroBIOS says "Homebrew (.nro, .nso) can run without keys or firmware" [X1] | Self-hosted Forgejo; GitHub `eden-emulator/Releases` = HTTP 451 per the 2026-02-12 Nintendo notice; `eden-emulator/Issue-Reports` still up [L1][E18] |
| **Ryubing** (Ryujinx fork, git.ryujinx.app/projects/Ryubing) | Maintenance mode since ~2026-02-16 (lead GreemDev stepped away; new maintainers "can't promise more than what Ryubing currently is") [R1][R2]. Last stable **1.3.3, 2025-10-11**; last confirmable canary 1.3.351, 2026-08-26; issue tracker active to Oct 2026 [R3][R4][R5] | Windows 10/11 x64, Linux x64/arm64 (tar + AppImage), macOS Apple Silicon [R6] | Yes: `Switch.LoadProgram -> ProcessLoader.LoadNxo`, ASET RomFS/NACP read, embedded `Homebrew.npdm` (ProgramId 0x010000000000100D), shader cache disabled for NROs [R7] | **No**: `KProcess.Start` sets X0=0, X1=main-thread handle (NSO-style); `LoadNxo` passes `arguments: null`; no CLI flag — unrecognised args are treated as the launch path [R7][R8] | `svcOutputDebugString` -> `Logger.Warning(LogClass.KernelSvc)`; headless has `--enable-gdb-stub` and log toggles; no nxlink code found [R9][R10] | `GraphicsBackend {Vulkan, OpenGl}` (Metal via MoltenVK on macOS); docs: OpenGL 4.5 / Vulkan 1.2 GPU required; AMD/Intel should use Vulkan [R6][R11] | nvdrv/nvdrv:a/s/t; nvhost-gpu SubmitGpfifo, MapCommandBuffer/Submit (Host1x), many channel ioctls stubbed-success; nvhost-ctrl SyncptWait + Event*; nvhost-as-gpu incl. GetVaRegions; nvhost-ctrl-gpu ZcullGetCtxSize/GetCharacteristics. Unknown ioctls -> `NvInternalResult.NotImplemented` [R12]. Open bugs: #263 unimplemented nvhost ioctl 0x80044715 crashes emulator ("low priority"), #264 NRO cannot access emulated SD card [R13][R14] | Not gated: NRO path never touches NCA decryption; docs only state keys+firmware for games [R6][R7] | GitHub source mirrors gone (404); Flathub repo 451 (2026-06-30 notice); Nintendo notices 2025-02-26, 2025-03-06, 2026-02-12 name Ryujinx/Ryubing [L1][L2][L3][L4] |
| **Kenji-NX** (Ryujinx fork, git.ryujinx.app/kenji-nx) | Android 2.1.0-pr.2 (~Mar 2026); desktop 2.0.5 seen Oct 2025; 2026 commit activity unverifiable (Forgejo unreachable) [K1][K2] | Windows (x64, arm64), macOS (x64, arm64), Linux, Android [K1][K3] | Presumably the Ryujinx loader (unverified) | No (inherits Ryujinx) | As Ryujinx | As Ryujinx | As Ryujinx | As Ryujinx | `Kenji-NX/Android-Releases` = 451 (2026-02-12 notice) [L1] |
| **Citron** (original, citron-emu.org) | Discontinued ~2026-02-16; site/Forgejo unreachable (NXDOMAIN) [C1][R1] | — | — | — | — | — | — | — | `*/Citron-AppImage` repos in the 2026-02-12 notice [L1] |
| **Citron Neo** (github.com/citron-neo/emulator) | Tagged releases to 2026-04-27; CI nightlies 2026-10-12 [C2][C3] | Windows x64, Linux x86_64/aarch64, Android arm64, macOS | Yes (old yuzu `nro.cpp`) | **No**: still yuzu's NSO-style `__argdata__` append, which libnx only reads in NSO mode [C4][Y1] | `OutputDebugString` -> `Debug_Emulated` | **Vulkan only** (OpenGL backend removed 2026-03-15) [C5] | yuzu lineage (same device set as Eden minus the 2026 homebrew fixes) | Not gated | GitHub-hosted; not in the Feb/Aug 2026 notices as of this date |
| **Sudachi** | Dead: domain parked, GitHub 404/451 (2024-07-05 notice) [S1] | — | — | — | — | — | — | — | — |
| **Suyu / Torzu / Nuzu / yuzu-mirror** | Suyu NXDOMAIN + 451; Torzu Tor-only, last code 2025-03; Nuzu last commit 2024-03; yuzu-mirror 451 (8,535-repo network, 2024-04-29) [S2][S3][S4][S5] | — | — | — | — | — | — | — | — |
| **Strato** (Skyline successor), **Pine**, **MeloNX** | Strato last commit 2024-04-28, "not yet ready for public release"; Pine/MeloNX taken down 2026-02-12 [S6][L1] | Android / iOS | — | — | — | — | — | — | — |

Why Eden over Ryubing for *this* workload: the Godot NRO drives the GPU through mesa's nvc0
driver, whose shaders are emitted by nv50_ir, not NVN. Eden's PR #4012 documents exactly that
difference ("Mesa shaders end with an unconditional EXIT instruction and no trailer … looped to its
maximum size"; fog/vertex-colour interpolation wrong) and fixes it; no equivalent fix or issue
exists for Ryujinx, whose only known homebrew tickets are a crash on an unimplemented ioctl and a
missing SD mount [E13][E14][R13][R14]. Eden is also the only emulator that can deliver argv to a
libnx NRO [E5], which matters for Godot's `--main-pack` / `--path` / `--verbose` style flags.

## Known divergences from real hardware

1. **Applet vs application mode.** On hardware, hbmenu launches NROs under nx-hbloader as
   `AppletType_LibraryApplet` (only `AppletType_SystemApplication + ApplicationOverride` when hbl
   replaces a title via Atmosphère `override_config.ini`) [H1][H2]. Both emulators create the NRO as
   an *application* process: Ryujinx ORs `ProcessCreationFlags.IsApplication` and sizes the resource
   limit to the Application region [R7]; Eden passes `kAppletTypeApplication = 0` and uses the
   default (Application) pool [E4][E19]. Consequences: libnx picks `appletOE` + service `"nvdrv"`
   instead of `appletAE` + `"nvdrv:a"` [H3][H4]; audio/video/library applets behave differently;
   anything that only breaks in the applet pool will not reproduce. Test applet-mode behaviour
   (album launch) on hardware.
2. **Memory limits.** Retail 4 GB arrangement: ApplicationPool 3285 MiB, AppletPool 507 MiB
   (mesosphere tables; `twib get-memory-info` on hardware shows "Applet Category Limit: 82 MiB /
   501 MiB") [H5][H6]. hbloader gives the NRO `TotalMemorySize - UsedMemorySize - 2 MiB` [H1], i.e.
   a few hundred MiB when launched from the album, ~3.2 GiB with title override. Emulators give the
   full Application pool and also offer "developer" layouts: Ryujinx 4/6/8 GiB ("Use alternative
   memory layout (Developers)") [R15], Eden 4/6/8/10/12 GiB with an 8 GiB application pool of
   6547 MiB ("Real kernel sets this to 4916_MiB. We are not debugging applets.") [E19]. Keep the
   emulator on 4 GiB and still expect OOMs to show up only on hardware.
3. **Loader config only when `-p` is non-empty (Eden).** Eden's `nro.cpp` writes the homebrew
   ConfigEntry table and argv only `if (!program_args.empty())`; otherwise X0=NULL and libnx runs in
   NSO mode (argc=1, argv[0]="", `AppletType_Default`) [E5]. Always pass `-p "<something>"` so the
   NRO takes the same libnx init path as under hbloader. Ryujinx never writes a loader config [R7].
4. **No HosVersion entry / firmware.** hbloader supplies `EntryType_HosVersion`; neither emulator
   does, so libnx falls back to `setsysGetFirmwareVersion` [H3]. Without dumped firmware the
   resulting hosversion may be 0 (unverified per emulator), which flips libnx to the pre-2.0.0
   `EventWait` fence path and `SubmitGpfifo` instead of `KickoffPb` [H7][H8]. Eden implements both
   submit ioctls; it does *not* implement nvhost-ctrl `SyncptWait` (0x16), which libnx does not use
   [E12][H7].
5. **GPU accuracy.** Both emulators recompile Maxwell shaders to host Vulkan/OpenGL and translate
   texture tiling/formats, so precision, format support and driver quirks are the host's, not the
   TX1's. Eden's Mesa-terminator heuristic is gated to non-proprietary (non-NVN) drivers because it
   broke retail titles [E14]; Eden's docs say "Intel and NVIDIA GPU support is limited" and RADV
   gets "first-class testing" — relevant for the developer's NVIDIA host [E20]. Ryujinx disables
   the shader cache for NROs [R7]. An open Eden issue shows `nwindowSetTransform` ROT_90/270 is
   ignored (`Unsupported framebuffer_transform_flags=4`) [E21]. deko3d upstream once required
   "Disable macro JIT" on yuzu (2021) — MME/macro emulation is a historical weak spot [Y2].
6. **CPU timing and clocks.** Hardware runs 1020 MHz (1785 MHz boost mode, 7.0.0+) with 384/768 MHz
   GPU profiles [H9]. Ryujinx derives the guest counter from a host `Stopwatch` with no CPU-clock
   model [R16]; Eden scales wall-clock by a configurable `cpu_clock` (1020/1734/2040 MHz) and
   `speed_limit`, and its "Unsafe FMA / Faster FRSQRTE / Faster ASIMD" options deliberately change
   float results [E22][E23]. Frame pacing, physics step timing and float determinism must be
   validated on hardware.
7. **Docked/handheld, sleep, HID.** Docked mode is a UI toggle in both (`GetOperationMode` reads a
   setting) [R17][E24]; sleep/suspend is not emulated (no code path found; HOS stubs
   `RequestToEnterSleep` for apps anyway) [H10]. Controller/HID is mapped from host devices.
8. **Filesystem and sdmc.** Emulators back `sdmc:` with a host folder (Eden expects
   `<eden folder>/sdmc/…`) [E6]; hardware hbmenu expects NROs under `switch/` on the SD card [H11].
   Ryubing #264 reports NROs cannot see the emulated SD at all [R14]. RomFS via ASET works in both
   loaders [E4][R7].
9. **stdout.** On hardware `nxlinkStdio()` dup2s stdout/stderr to the host's `nxlink -s` TCP socket
   (port 28771) when the `_NXLINK_` argv marker is present [H12][H13][H14]. No emulator provides
   that today (Eden #4233 pending) [E8]; `printf` output is lost unless the port also logs through
   `svcOutputDebugString`. Build the Godot port's logger to mirror stdout to
   `svcOutputDebugString` under a `--nx-debug` flag so one code path serves both targets.

## Setup notes for WSL2 / Windows

- **Do not run the emulator inside WSL2.** WSLg exposes OpenGL via Mesa's `d3d12` Gallium driver,
  which "can be used to get full desktop OpenGL 3.3 support" — `glxinfo` in WSLg reports "Max core
  profile version: 3.3" [W1][W2]; Eden needs OpenGL 4.6 or Vulkan 1.1 [E10], Ryujinx OpenGL 4.5 or
  Vulkan 1.2 [R6]. WSLg's README says nothing about Vulkan; Mesa's `dzn` (Vulkan-on-D3D12) exists
  since Mesa 22.1 but is non-conformant and reportedly missing from Ubuntu's `mesa-vulkan-drivers`
  [W3][W4]. WSLg also routes vGPU output "through system memory", with overhead "as high as 50%"
  [W1].
- **Build in WSL2, run on Windows.** Download the Windows zip (MSVC amd64 or amd64-v3) from
  `https://git.eden-emu.dev/eden-emu/eden/releases` or a nightly from
  `https://git.eden-emu.dev/eden-ci/nightly/releases` (the GitHub mirror is 451) [E2][E3][E18].
  Launch the NRO from the WSL filesystem via `\\wsl.localhost\<distro>\home\...\godot.nro` (or copy
  to `/mnt/c/...`). Pass arguments with `-p "<args>"` (see `docs/user/CommandLine.md`) or the
  Debug > program arguments field in the Qt UI; remember argv[0] is `homebrew` and user args start
  at argv[1] [E5][E6].
- **Logging.** Enable the `Debug.Emulated` log class (Eden log file / console) to see
  `svcOutputDebugString` output [E7]. `-hlaunch` (nx-hbloader via `sdmc/atmosphere/hbl.nsp`) is
  only for reproducing hbmenu's applet launch; Eden says it does not support CFW beyond HLE
  compatibility [E6][E25].
- **Memory layout.** Leave Eden on `Memory_4Gb`; do not use 8/12 GiB developer layouts for
  OOM-sensitive tests [E19].
- **Second opinion.** Keep Ryubing 1.3.3 (Windows zip from the Forgejo releases) for a differing
  nvdrv/GPU implementation when Eden misbehaves; there is no argv, so bake defaults into the NRO
  for that run [R3][R7].
- **Ground truth.** Anything touching memory ceilings, applet mode, nxlink, timing or NVIDIA-driver
  specific shader output must be re-verified on the modded Switch.

## Sources

Eden
- [E1] Repo + latest commit: https://git.eden-emu.dev/eden-emu/eden (commit 218c6705, 2026-10-13)
- [E2] Releases: https://git.eden-emu.dev/eden-emu/eden/releases (v0.2.1 2026-06-01, "hotfix release for 0.2.0")
- [E3] Nightlies: https://git.eden-emu.dev/eden-ci/nightly/releases
- [E4] NRO loader: https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/src/core/loader/nro.cpp
- [E5] PR #4013 homebrew argv: https://git.eden-emu.dev/eden-emu/eden/pulls/4013
- [E6] CLI docs (`--program/-p`, `-hlaunch`, `--null-render`): https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/docs/user/CommandLine.md
- [E7] svcOutputDebugString: https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/src/core/hle/kernel/svc/svc_debug_string.cpp ; PRs #3744, #4291
- [E8] PR #4233 nxlink/NextLoad (draft): https://git.eden-emu.dev/eden-emu/eden/pulls/4233
- [E9] Renderer enum: `src/common/settings_enums.h` on master
- [E10] System requirements: https://eden-emu.dev/system-requirements
- [E11] PR #4342 drop OpenGL on Android: https://git.eden-emu.dev/eden-emu/eden/pulls/4342
- [E12] nvdrv devices: https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/src/core/hle/service/nvdrv/nvdrv.cpp and `devices/nvhost_gpu.cpp`, `nvhost_ctrl.cpp`, `nvhost_as_gpu.cpp`, `nvhost_ctrl_gpu.cpp`, `nvmap.cpp`
- [E13] PR #4012 Mesa shader fixes: https://git.eden-emu.dev/eden-emu/eden/pulls/4012
- [E14] PR #4151 gate Mesa EXIT detection: https://git.eden-emu.dev/eden-emu/eden/pulls/4151
- [E15] PR #4061 ETC2: https://git.eden-emu.dev/eden-emu/eden/pulls/4061
- [E16] PR #3983 fence id -1: https://git.eden-emu.dev/eden-emu/eden/pulls/3983
- [E17] PR #4264 homebrew service stubs: https://git.eden-emu.dev/eden-emu/eden/pulls/4264 ; PR #3771 sm:AtmosphereHasService
- [E18] GitHub mirror status: https://github.com/eden-emulator/Releases (HTTP 451); https://github.com/eden-emulator/Issue-Reports
- [E19] Memory pools: https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/src/core/hle/kernel/board/nintendo/nx/k_system_control.cpp ; `src/core/file_sys/program_metadata.cpp`
- [E20] Architectures/driver support: `docs/user/Architectures.md` on master
- [E21] Issue-Reports #661 nwindowSetTransform: https://github.com/eden-emulator/Issue-Reports/issues/661
- [E22] Core timing: https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/src/core/core_timing.cpp ; `src/common/settings.h`
- [E23] Settings doc (unsafe CPU options): https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/docs/user/Settings.md
- [E24] Docked mode: https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/src/core/hle/service/am/service/common_state_getter.cpp
- [E25] CFW stance: `docs/user/CFW.md` on master

Ryujinx / Ryubing / Kenji-NX
- [R1] Maintenance-mode announcement (secondary relays of the developer post): https://x.com/SwitchTools/status/2023496310968442978 ; https://x.com/GBAtemp/status/2023806972722925972 ; https://gbatemp.net/threads/switch-emulator-citron-is-being-discontinued-and-ryubing-enters-maintenance-mode.679777/
- [R2] Ryubing org (source mirror gone; Issues/docs/UpdateServer remain): https://github.com/Ryubing
- [R3] 1.3.3 release 2025-10-11 (Codeberg archive of releases): https://codeberg.org/Software_arch/Ryubing/releases ; Flathub appstream: https://flathub.org/api/v2/appstream/io.github.ryubing.Ryujinx
- [R4] Canary mirror (1.3.351, 2026-08-26): https://github.com/ewigl/ryubing-canary-mirror/releases
- [R5] Issue tracker: https://github.com/Ryubing/Issues/issues
- [R6] Setup guide / requirements: https://docs.ryujinx.app/guides/setup-guide/ ; downloads: https://ryujinx.app/download
- [R7] Loader code (upstream snapshot via mirror): `src/Ryujinx.HLE/Switch.cs`, `Loaders/Processes/ProcessLoader.cs`, `ProcessLoaderHelper.cs`, `Extensions/MetaLoaderExtensions.cs`, `HOS/Kernel/Process/KProcess.cs` — https://git.axenov.dev/Museum/ryujinx (upstream to 2024-10-24) and https://github.com/Stella-sea/ryujinx-admin (Ryubing mirror, synced 2026-08-26)
- [R8] CLI handling: `src/Ryujinx/Utilities/CommandLineState.cs`, `src/Ryujinx/Headless/Options.cs` (same mirrors)
- [R9] `svcOutputDebugString`: `src/Ryujinx.HLE/HOS/Kernel/SupervisorCall/Syscall.cs`
- [R10] Headless log/gdb flags: `src/Ryujinx/Headless/Options.cs`
- [R11] FAQ (homebrew statement, Vulkan on AMD/Intel, applets, NRO shader cache): https://docs.ryujinx.app/info/faq-&-troubleshooting/
- [R12] nvdrv: `src/Ryujinx.HLE/HOS/Services/Nv/INvDrvServices.cs`, `NvDrvServices/NvHostChannel/NvHostChannelDeviceFile.cs`, `NvHostCtrl/…`, `NvHostAsGpu/…`, `NvHostCtrlGpu/…`, `NvMap/…`
- [R13] Issue #263 unimplemented nvhost ioctl: https://github.com/Ryubing/Issues/issues/263
- [R14] Issue #264 NRO cannot access SD: https://github.com/Ryubing/Issues/issues/264
- [R15] Memory layouts: `src/Ryujinx.HLE/MemoryConfiguration.cs`, `HOS/Kernel/Common/KSystemControl.cs`; commit https://git.axenov.dev/Museum/ryujinx/commit/ab0491817e87b9dd134c41764ba213f8c8559e9b
- [R16] Tick source: `src/Ryujinx.Cpu/TickSource.cs`
- [R17] Docked mode: `src/Ryujinx.HLE/HOS/Services/Am/AppletAE/AllSystemAppletProxiesService/SystemAppletProxy/ICommonStateGetter.cs`
- [K1] Kenji-NX org README: https://github.com/Kenji-NX ; issues: https://github.com/Kenji-NX/Issues
- [K2] Android 2.1.0-pr.2 (secondary): https://retrohandhelds.gg/retro-handhelds-weekly-edition-92/
- [K3] Platform builds (cached release note): https://newreleases.io/project/github/Kenji-NX/Kenji-NX/release/51f26f8

Citron / Sudachi / others
- [C1] Citron disappearance (secondary): https://retrohandhelds.gg/citron-popular-switch-emulator-is-gone-but-its-not-nintendos-fault/
- [C2] Citron Neo repo/releases: https://github.com/citron-neo/emulator/releases
- [C3] Citron Neo nightlies: https://github.com/citron-neo/CI/releases
- [C4] Citron Neo `src/core/loader/nro.cpp` (yuzu argdata path)
- [C5] Citron Neo PR #2 remove OpenGL: https://github.com/citron-neo/emulator/pull/2
- [S1] Sudachi: https://github.com/github/dmca/blob/master/2024/07/2024-07-05-nintendo.md ; archive https://codeberg.org/bigjuan/sudachi
- [S2] Suyu network takedown: https://github.com/github/dmca/blob/master/2026/08/2026-08-17-nintendo.md
- [S3] Torzu archive: https://codeberg.org/Software_arch/Torzu
- [S4] Nuzu: https://github.com/qqq26/Nuzu
- [S5] yuzu-mirror takedown: https://github.com/github/dmca/blob/master/2024/04/2024-04-29-nintendo.md
- [S6] Strato: https://github.com/strato-emu/strato
- [Y1] yuzu NSO-style argdata (Torzu archive): https://codeberg.org/Software_arch/Torzu/raw/branch/master/src/core/loader/nro.cpp
- [Y2] deko3d #8 "Deko3D not working with Yuzu": https://github.com/devkitPro/deko3d/issues/8 ; yuzu Oct 2022 report: https://yuzu-mirror.github.io/entry/yuzu-progress-report-oct-2022/

Hardware / libnx / devkitPro
- [H1] nx-hbloader main.c (applet type, heap sizing, ApplicationOverride): https://raw.githubusercontent.com/switchbrew/nx-hbloader/master/source/main.c ; hbl.json: https://raw.githubusercontent.com/switchbrew/nx-hbloader/master/hbl.json
- [H2] Atmosphère override config: https://raw.githubusercontent.com/Atmosphere-NX/Atmosphere/master/config_templates/override_config.ini ; https://raw.githubusercontent.com/Atmosphere-NX/Atmosphere/master/docs/features/configurations.md
- [H3] libnx env/applet/init: https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/runtime/env.c ; https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/services/applet.c ; https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/runtime/init.c
- [H4] libnx nv service selection: https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/services/nv.c ; https://switchbrew.org/wiki/NV_services
- [H5] mesosphere pool sizes: https://raw.githubusercontent.com/Atmosphere-NX/Atmosphere/master/libraries/libmesosphere/source/board/nintendo/nx/kern_k_system_control.cpp
- [H6] twili memory info on hardware: https://raw.githubusercontent.com/misson20000/twili/master/README.md
- [H7] libnx fences: https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/nvidia/fence.c
- [H8] libnx GPU channel (KickoffPb vs SubmitGpfifo): https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/nvidia/gpu_channel.c
- [H9] Clock profiles: https://switchbrew.org/wiki/PPC_services
- [H10] Applet manager services (OperationMode, sleep): https://switchbrew.org/wiki/Applet_Manager_services
- [H11] Homebrew applications layout: https://switchbrew.org/wiki/Homebrew_Applications ; hbmenu netloader: https://switchbrew.org/wiki/Homebrew_Menu
- [H12] nxlink client: https://raw.githubusercontent.com/switchbrew/libnx/master/nx/include/switch/runtime/nxlink.h ; https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/runtime/nxlink_stdio.c ; https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/runtime/nxlink.c ; argv: https://raw.githubusercontent.com/switchbrew/libnx/master/nx/source/runtime/argv.c
- [H13] hbmenu `_NXLINK_` marker + netloader: https://raw.githubusercontent.com/switchbrew/nx-hbmenu/master/nx_main/loaders/builtin.c ; https://raw.githubusercontent.com/switchbrew/nx-hbmenu/master/common/netloader.c
- [H14] host nxlink tool (`-s` server mode): https://raw.githubusercontent.com/switchbrew/switch-tools/master/src/nxlink.c
- Homebrew ABI: https://switchbrew.org/wiki/Homebrew_ABI
- devkitPro mesa / libdrm_nouveau / deko3d GPU path: https://github.com/devkitPro/mesa (branch switch-20.1.0-rc3, `src/gallium/winsys/nouveau/switch/nouveau_switch_winsys.c`) ; https://raw.githubusercontent.com/devkitPro/libdrm_nouveau/master/source/nouveau.c ; https://github.com/devkitPro/deko3d (`dk_device.cpp`, `dk_queue.cpp`, `dk_swapchain.cpp`) ; https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/mesa/PKGBUILD
- switch-examples OpenGL sample (`-lglad -lEGL -lglapi -ldrm_nouveau -lnx`, nxlinkStdio): https://raw.githubusercontent.com/switchbrew/switch-examples/master/graphics/opengl/simple_triangle/Makefile ; .../source/main.cpp

Legal
- [L1] Nintendo notice 2026-02-12 (names Citron, Eden, Kenji-NX, MeloNX, Pine, Pomelo, Ryubing, Ryujinx, Skyline, Sudachi, Sumi, Suyu, Yuzu; 17 U.S.C. § 1201; fork networks of 127 and 218 repos): https://github.com/github/dmca/blob/master/2026/02/2026-02-12-nintendo.md
- [L2] 2025-02-26 and 2025-03-06 Ryujinx notices: https://github.com/github/dmca/blob/master/2025/02/2025-02-26-nintendo.md ; https://github.com/github/dmca/blob/master/2025/03/2025-03-06-nintendo.md
- [L3] 2026-06-30 Flathub Ryubing notice: https://github.com/github/dmca/blob/master/2026/06/2026-06-30-nintendo.md
- [L4] Ryujinx Oct 2024 shutdown (secondary): https://en.wikipedia.org/wiki/Ryujinx ; https://www.theverge.com/2024/10/1/24259791/nintendo-ryujinx-switch-emulator-gdkchan-removed-downloads-github
- Nintendo v. Tropic Haze (yuzu) final judgment: https://storage.courtlistener.com/recap/gov.uscourts.rid.56980/gov.uscourts.rid.56980.11.0.pdf ; docket https://www.courtlistener.com/docket/68284505/nintendo-of-america-inc-v-tropic-haze-llc/

WSL2 / Windows
- [W1] WSLg README (d3d12 Gallium OpenGL, system-memory overhead, no Vulkan mention): https://github.com/microsoft/wslg/blob/main/README.md
- [W2] WSLg GPU selection wiki (`glxinfo`: "Max core profile version: 3.3"): https://github.com/microsoft/wslg/wiki/GPU-selection-in-WSLg ; Mesa d3d12 driver docs ("full desktop OpenGL 3.3 support"): https://docs.mesa3d.org/drivers/d3d12.html
- [W3] Mesa 22.1 release notes (dzn merged): https://docs.mesa3d.org/relnotes/22.1.0.html
- [W4] dzn non-conformant / missing from Ubuntu packages (secondary user reports): https://forums.developer.nvidia.com/t/vulkan-fails-to-detect-nvidia-gpu-on-wsl2-ubuntu-24-04-dzn-driver-files-missing-tested-on-multiple-systems/342142 ; https://github.com/ggml-org/llama.cpp/discussions/26729
- Microsoft WSL GUI apps doc (vGPU driver prerequisite): https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps

Secondary (flagged)
- [X1] RetroBIOS Eden profile ("Homebrew (.nro, .nso) can run without keys or firmware"): https://abdess.github.io/retrobios/emulators/eden/
