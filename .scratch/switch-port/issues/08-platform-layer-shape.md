# Platform layer shape for the first milestone

Type: grilling
Status: open
Blocked by: 01, 03, 04
Map: ../map.md

## Question

Which Godot interfaces does `platform/switch` implement for the first milestone, and how are they split: `OS_Switch` (main loop around `appletMainLoop`, `user://`/`res://` roots, time, env, process-less), `DisplayServerSwitch` (single window, EGL context), `AudioDriverSwitch`, `JoypadSwitch`/touch feeding `Input`, plus `platform_config.h` and `godot_switch.cpp` entry. What does the file layout look like, mirroring `platform/linuxbsd` vs `platform/web`? Which `OS` methods are stubbed vs implemented?

Context: the prior-art ticket recommends starting from Ozuba/godot `switch-platform` (see [research/03-prior-art.md](../research/03-prior-art.md)); decide what to keep, what to replace, and what to add from Stary2001 (applet-mode guard).
Context: the libnx-runtime ticket lists the exact shims (`platform_thread.h` stacks, clock, cpu count, missing process calls, no dlopen, crash handler off); see [research/04-libnx-runtime.md](../research/04-libnx-runtime.md).
