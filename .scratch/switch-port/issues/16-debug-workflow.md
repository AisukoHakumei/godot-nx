# Debug workflow: nxlink, logs, crashes

Type: grilling
Status: open
Blocked by: 07, 08
Map: ../map.md

## Question

How do Godot's `print`/`ERR_*` outputs reach the developer: `nxlinkStdio` when launched via `nxlink`, a log file on SD otherwise? How is `OS::alert` handled? What does a crash look like on hardware (Atmosphère fatal report) and on the emulator, and what is the minimum the first milestone needs to make a crash diagnosable?

Context: `nxlinkConnectToHost` does not set stdout buffering; the port must call `setvbuf` itself (see [research/04-libnx-runtime.md](../research/04-libnx-runtime.md)). No `backtrace` is available, so crash diagnosis cannot rely on Godot's crash handler.
Context: Eden does not emulate nxlink but logs `svcOutputDebugString`; hardware uses nxlink. The workflow needs both sinks; see [research/07-emulator.md](../research/07-emulator.md).
