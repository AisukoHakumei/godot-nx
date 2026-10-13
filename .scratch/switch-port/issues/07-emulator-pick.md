# Emulator for fast iteration

Type: research
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

As of October 2026, which Switch emulator (Ryujinx forks such as Ryubing, yuzu forks such as Eden or Citron, others) runs devkitPro mesa/EGL OpenGL homebrew most faithfully on Linux/WSL2 or Windows, supports loading an NRO with arguments or `nxlink`-style stdout, and is actively maintained? Recommend one and note known divergences from real hardware that could mislead testing. Cite project repos and docs.

## Answer

**Eden** (stable v0.2.1, 2026-06-01, or a nightly), run natively on the Windows host, not inside WSL2.

- Only candidate that is actively maintained (commits in the week of 2026-10-13), ships Windows and Linux builds, has an NRO loader honouring the libnx homebrew ABI for argv (PR #4013, merged 2026-06-15), and carries explicit fixes for Mesa nv50_ir-compiled shaders (PRs #4012/#4151), which is the devkitPro mesa path.
- Eden only emits the loader config when `-p` is non-empty: always pass `-p "<something>"` to get hbloader-like libnx init.
- Ryubing (Ryujinx fork) is in maintenance mode since Feb 2026, starts NROs NSO-style with no argv, and has open homebrew bugs (NRO cannot see the SD card; an unimplemented nvhost ioctl crashes it). Second opinion only.
- Citron, Sudachi, Suyu, Torzu, yuzu-mirror are dead, Vulkan-only, or Tor-only.
- No emulator implements nxlink; both expose `svcOutputDebugString` in their log (Eden: `Debug.Emulated`). An Eden nxlink-server PR (#4233) is open but unmerged. The debug-workflow ticket must plan for `svcOutputDebugString` on emulator and nxlink on hardware.
- NROs run without prod.keys or firmware. Nintendo's 2026-02-12 GitHub DMCA notice took down Eden/Kenji-NX GitHub mirrors; fetch Eden only from git.eden-emu.dev.
- Divergences from hardware: emulators run the NRO as an application (3.2 GiB pool, `nvdrv`, `appletOE`) whereas hbmenu on hardware runs it as a library applet (~500 MiB pool, `nvdrv:a`); wall-clock-scaled CPU timing; host-driver shader recompilation; Eden calls NVIDIA support "limited" (RADV is first-class).
- WSL2/WSLg caps OpenGL at 3.3 and its Vulkan is non-conformant: build in WSL2, run the Windows Eden build on `\\wsl.localhost\...` paths.

Findings: [research/07-emulator.md](../research/07-emulator.md) (branch `research/emulator`, commit 4345d9c536).
