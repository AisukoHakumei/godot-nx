# libnx runtime vs Godot 4.7 core requirements

Type: research
Status: resolved
Blocked by: (none)
Map: ../map.md

## Question

Does devkitA64's newlib + libnx satisfy what Godot 4.7 core needs: `std::thread`/pthreads, `thread_local`, 64-bit atomics, `std::filesystem` or POSIX `stat`/`opendir`, `mmap` (or Godot's fallbacks), clock functions, `getenv`, large per-thread stacks, and no `dlopen`? What are the memory budgets in application mode vs applet mode, and how are they set (`__nx_applet_type`, heap override)? Answer from libnx headers/source and devkitPro docs, with file citations.

## Answer

**Yes, with a short list of known shims.** Verified by linking a probe against `-specs=switch.specs -lnx`, not just by reading docs.

Works out of the box: `std::thread`/mutex/shared_mutex/condition_variable, dynamic `thread_local`, lock-free 64-bit atomics, `clock_gettime`/`gettimeofday`/`nanosleep`, everything `file_access_unix.cpp` and `dir_access_unix.cpp` call, `getenv`/`setlocale`/`gethostname`/`getpid`/`signal`, and the BSD socket API including `getaddrinfo`/`poll`.

Required shims and patches:
1. **Thread stacks**: libnx gives every pthread 128 KiB and `std::thread` passes NULL attrs; the hbloader main thread has 1 MiB. Provide a `PLATFORM_THREAD_OVERRIDE` `platform_thread.h` that sets 1-2 MiB stacks (as Ozuba did), and decide whether the engine runs off the main thread.
2. **Clock**: newlib defines `CLOCK_MONOTONIC_RAW` but libnx only implements MONOTONIC/REALTIME, so `GODOT_CLOCK` in `os_unix.cpp` picks a clock that fails. One-line patch.
3. **CPU count**: `std::thread::hardware_concurrency()` returns 0 here (`sysconf` is unlinkable); override `get_processor_count()` to 3 (hbloader `highest_cpu_id: 2`). Otherwise WorkerThreadPool gets no workers.
4. **No thread naming/priority/affinity** (`pthread_setname_np`, `pthread_setschedparam`, `cpu_set_t` absent): stub.
5. **Missing process/system calls**: `sigaction`, `getrlimit`, `uname`, `sysconf`, `pipe`, `mkfifo`, `popen`, `execvp`, `waitpid`, `setsid`, `getentropy`, `fdatasync`, `mmap`. `os_unix.cpp` and `file_access_unix_pipe.cpp` need `#ifdef` cuts, or `OS_Switch` avoids inheriting those paths. `<sys/xattr.h>` include must be guarded; `chmod` returns `ENOSYS`; `d_type`/`st_ino` are always 0.
6. **No `dlopen`**: GDExtension fails softly at runtime (`ERR_UNAVAILABLE`); the non-glibc `_load_iconv()` path must be cut; build with `use_volk=no`, `use_sowrap=no`, no Vulkan.
7. **Crash handler**: no `execinfo.h`/`backtrace`; leave `CRASH_HANDLER_ENABLED` off and stub `CrashHandler`.
8. **Networking** (later milestone): missing `<sys/un.h>`, `<ifaddrs.h>`, `getifaddrs`, `if_nametoindex`, `struct ipv6_mreq`; `socketInitialize()` must run first.

Memory: heap auto-sizes to total minus used minus 2 MiB; under hbloader it arrives via `EntryType_OverrideHeap`, so `__nx_heap_size` is ignored. Retail 4 GB unit pools: application mode roughly 3285 MiB, applet pool 507 MiB shared by all applets. `nxlinkConnectToHost` only `dup2`s the socket, so the port must `setvbuf(stdout, NULL, _IOLBF, 0)` itself.

Findings: [research/04-libnx-runtime.md](../research/04-libnx-runtime.md) (branch `research/libnx-runtime`, commit 4f5ca2e5d4).
