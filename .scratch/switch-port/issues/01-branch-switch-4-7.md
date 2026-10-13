# Create the `switch-4.7` branch from `4.7.2-stable`

Type: task
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

Fetch upstream `godotengine/godot` tag `4.7.2-stable` into this fork and create branch `switch-4.7` from it, so every later ticket reads the 4.7 platform code rather than master (4.8 dev). The `.scratch/` tracker and `CONTEXT.md` are untracked and carry over; commit them on the new branch. Record the resulting commit hash and confirm `scons platform=list` style discovery still works on that tag. AFK: the agent can do this alone.

## Answer

Done on 2026-10-13.

- Added remote `upstream` = `https://github.com/godotengine/godot.git` and fetched only the tag `4.7.2-stable` (commit `ed1daf0bf001b61586d9930840f2f1394092c079`, "Bump version to 4.7.2-stable").
- Created and checked out branch `switch-4.7` from that tag. `version.py` reads 4.7.2 stable. `platform/` contains android, ios, linuxbsd, macos, visionos, web, windows; no switch yet.
- Committed `.scratch/switch-port/` and `CONTEXT.md` on the branch as `1904f2680f`. `.claude/` (research worktrees) is untracked and must stay uncommitted.
- Platform discovery on this tag (`SConstruct` lines 70-105): SCons globs `platform/*/detect.py`, imports each, and registers the platform if `detect.can_build()` is true; `platform/<name>/export/export.cpp` registers an exporter and `platform/<name>/api/api.cpp` a platform API. A `platform/switch/detect.py` with `can_build()` checking `DEVKITPRO` is enough to appear in `scons platform=<bad>`'s "available platforms" list.
- Fact for later tickets: `scons` is not installed in this WSL2 environment (`python3 -c "import SCons"` fails). Godot 4.7 needs SCons 4.0+; install it (`pip install --user scons` or the distro package) before the build-configuration ticket is acted on. Added to the portlibs task's checklist.
- The five `research/*` branches were cut from master, not from this branch; their findings are already copied into `.scratch/switch-port/research/` on `switch-4.7`, so those branches are now disposable.
