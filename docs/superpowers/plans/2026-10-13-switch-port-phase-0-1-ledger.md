# SDD ledger — plan: docs/superpowers/plans/2026-10-13-switch-port-phase-0-1.md
Spec: docs/superpowers/specs/2026-10-13-switch-port-design.md (read)
Setup: Ruling: work in place on branch switch-4.7 (the dedicated port branch, not master); no separate worktree — hardware verification steps need the user to find bin/ outputs where they expect them — cost if wrong: none beyond a dirty working tree, branch is not shared.
Pre-flight:
- T4 produces SwitchHelpers::{executable_path,user_dir_for_executable,pck_candidates,touch_to_window,stick_axis,mix_to_int16,window_size_for_docked}; consumed by T5,T7,T9,T10 — names match. T7 adds missing_pck_message, T10 adds audout_buffer_bytes/audout_block_bytes — declared in their own tasks. OK.
- T6 produces DisplayServerSwitch::set_resize_pending/set_focus_pending/set_resume_pending; consumed by T8 — OK.
- T8 consumes AudioDriverSwitch::set_paused which T10 produces later. Ruling: T8 leaves focus->audio wiring out; T10 adds the call in process_events when it lands — cost if wrong: one missed line, caught by T10's sleep/resume check.
- T1 detect.py post-action consumes tools/default_icon.jpg produced in T12 — plan already says omit --icon when absent. OK.
- T5 OS_Switch::initialize() must call DisplayServerSwitch::register_switch_driver() (T6) — T6 modifies os_switch.cpp. OK.
Task 1: Ruling: logo.svg/run_icon.svg skipped — 4.7 platforms no longer ship them and templates do not need them — cost if wrong: none for template builds. Ruling: test loader adds repo root to sys.path so detect.py can import methods as it does under SCons — cost if wrong: none.
Task 1: complete (commits 622b8b7..c35c802, tests: python -I -m pytest tests/python_build/test_switch_detect.py -q → 2 passed in 0.01s)
Task 2: Ruling: object targets are bin/obj/<src>.switch.template_debug.arm64.o, not <src>.… as the plan wrote — cost if wrong: none, naming only.
Task 2: Ruling: detect.py lacked env.Prepend(CPPPATH=['#platform/switch']) (Task 1 defect, fixed here) — cost if wrong: none.
Task 2: Ruling: PLATFORM_THREAD_OVERRIDE makes core/os/thread.h require platform_thread.h, so Task 3 is executed before Task 2's verification completes — cost if wrong: none, ordering only.
Task 3: Ruling: executed before Task 2 completes (see Task 2 ruling); set_name is a no-op because libnx lacks pthread_setname_np — cost if wrong: thread names invisible in debuggers only.
Task 3: complete (commits c35c802..706048d, tests: scons platform=switch target=template_debug bin/obj/platform/switch/platform_thread.switch.template_debug.arm64.o bin/obj/core/os/thread.switch.template_debug.arm64.o → INFO: Time elapsed: 00:00:00.04)
Task 2: Ruling: the .h files of both Unix drivers carry the same UNIX_ENABLED guard and were extended too (plan listed only the .cpp) — cost if wrong: none.
Task 2: complete (commits c35c802..3a6aa3c, tests: scons platform=switch target=template_debug bin/obj/drivers/unix/file_access_unix.switch.template_debug.arm64.o bin/obj/drivers/unix/dir_access_unix.switch.template_debug.arm64.o bin/obj/drivers/gles3/storage/render_scene_buffers_gles3.switch.template_debug.arm64.o bin/obj/drivers/gles3/storage/texture_storage.switch.template_debug.arm64.o bin/obj/drivers/gles3/storage/config.switch.template_debug.arm64.o → INFO: Time elapsed: 00:00:00.08)
Task 5: Ruling: libnx <switch.h> types Thread/Mutex/RMutex/Semaphore/CondVar collide with Godot classes; platform sources include libnx via platform/switch/switch_wrapper.h which prefixes them Nx for the libnx headers only; the 8 MiB engine thread uses pthread_attr_setstacksize instead of threadCreate for the same reason — cost if wrong: a rename in one header.
Task 4: complete (commits 3a6aa3c..de3af65, tests: bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case=*SwitchHelpers* → [doctest] Status: SUCCESS!)
Task 6: Ruling: OS::initialize_core is not virtual in 4.7; OS_Switch calls its own initialize_core() from its constructor like OS_LinuxBSD; DisplayServer::make_rendering_thread does not exist, dropped — cost if wrong: none.
Task 6: Ruling: thirdparty/manifold/src/svd.h gets '#undef _S' because newlib <ctype.h> defines _S; alternative was disabling module_csg which the game may use — cost if wrong: a 4-line thirdparty diff to carry on upgrades. initialize_core() moved into initialize() to match OS_LinuxBSD.
Task 6: Ruling: modules/jolt_physics/SCsub defines JPH_PLATFORM_BSD on switch (Jolt's chain has no Horizon case; the BSD branch needs only pthread semaphores, which newlib provides) — cost if wrong: Jolt thread naming/tick counter fallbacks, no functional loss.
Task 6: Ruling: modules/mbedtls/SCsub drops net_sockets.c on switch (MBEDTLS_NET_C stays defined, nothing references mbedtls_net_*) — cost if wrong: a link error naming mbedtls_net_*.
Task 6: Ruling: MBEDTLS_PLATFORM_MS_TIME_ALT under __SWITCH__ in godot_module_mbedtls_config.h, implemented in modules/mbedtls/mbedtls_ms_time_switch.cpp — cost if wrong: TLS timing only.
Task 6: Ruling: superseded the MS_TIME_ALT ruling — thirdparty/mbedtls/library/common.h lists __SWITCH__ as Unix-like (its documented extension point) so timing.c and platform_util.c compile as on Linux; mbedtls_ms_time_switch.cpp removed; net_sockets.c stays excluded — cost if wrong: one-line thirdparty diff to carry.
Task 6: Ruling: MS_TIME_ALT restored (platform_util.c keys its clock on __linux__/Apple/Windows, not the Unix-like flag); both the common.h line and the ALT clock stay — cost if wrong: TLS timing only.
Task 6: Ruling: module_raycast_enabled=False on switch (embree's sse2neon/avx2neon headers fail under GCC aarch64; web does the same) — cost if wrong: no runtime occlusion culling on Switch; revisit with clang or an embree patch later.
Task 6: Ruling: builtin_pcre2_with_jit=False on switch (sljit needs sys/mman.h) — cost if wrong: slower RegEx only.
Task 6: Ruling: platform/switch/switch_libc_shims.cpp provides posix_memalign over newlib memalign — cost if wrong: none, same allocator.
Task 5: complete (commits de3af65..40f2a39, tests: scons platform=switch target=template_debug bin/obj/platform/switch/os_switch.switch.template_debug.arm64.o bin/obj/platform/switch/godot_switch.switch.template_debug.arm64.o bin/obj/platform/switch/switch_logger.switch.template_debug.arm64.o → INFO: Time elapsed: 00:00:00.17)
Task 6: Ruling: steps 5-6 (Eden/hardware) handed to the human; later tasks proceed build-verified in the meantime so one hardware session covers them; Task 6 ledger line is withheld until the human reports — cost if wrong: rework of GL-dependent code if phase 0 fails.
Task 7: Ruling: the pck existence check in main() uses fopen, not FileAccess, because FileAccess defaults are installed by OS_Switch::initialize() inside Main::setup — cost if wrong: none. Step 5 (hardware) deferred to the human session like Task 6.
Task 7: complete (commits 86356e4..be9771d, tests: bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case=*SwitchHelpers* → [doctest] Status: SUCCESS!)
Task 8: Ruling: hook is a file-static free function with libnx's exact signature (header stays libnx-free); audio pause wiring arrives with Task 10; step 3 (hardware) deferred to the human session.
Task 8: complete (commits be9771d..c4eeb15, tests: scons platform=switch target=template_debug bin/obj/platform/switch/display_server_switch.switch.template_debug.arm64.o → INFO: Time elapsed: 00:00:00.16)
Task 9: Ruling: joypad reports an empty GUID (buttons are already in Godot's logical layout; the plan's SDL GUID would trigger a database remap on top); DisplayServerSwitch constructs JoypadSwitch (spec: the display server owns it) and OS_Switch::initialize_joypads stays empty; hardware step deferred.
Task 9: complete (commits c4eeb15..7d8dd79, tests: scons platform=switch target=template_debug bin/obj/platform/switch/joypad_switch.switch.template_debug.arm64.o → INFO: Time elapsed: 00:00:00.13)
Task 10: Ruling: libnx audio/driver.h also names a C type AudioDriver; renamed in switch_wrapper.h like Thread/Mutex. Proof scene toggles a generated 440 Hz tone with Switch B instead of a separate one-shot; hardware step deferred.
Task 10: complete (commits 7d8dd79..47d0c7d, tests: bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case=*SwitchHelpers* → [doctest] Status: SUCCESS!)
Task 11: Ruling: user directories are created in OS_Switch::initialize() after initialize_core() (DirAccess is available there; the plan said main()); file logging enabled through a GLOBAL_DEF feature override 'enable_file_logging.switch' in main.cpp, mirroring the existing '.pc' line; hardware step deferred.
Task 11: complete (commits 47d0c7d..a7da805, tests: scons platform=switch target=template_debug bin/obj/platform/switch/os_switch.switch.template_debug.arm64.o → INFO: Time elapsed: 00:00:00.14)
Task 12: Ruling: default_icon.jpg generated from editor/icons/Godot.svg with the editor binary (no PIL/ImageMagick here); the real packaging run used the debug template via --elf since no release template is built yet; Eden boot of the packaged NRO deferred to the human session.
Task 12: complete (commits 47d0c7d..b78874a, tests: python -I -m pytest tests/python_build/test_switch_package.py tests/python_build/test_switch_detect.py -q → 4 passed in 0.02s)
Final review: fresh reviewer (fable), package review-622b8b78b3..b78874a575.diff. Verdict: with fixes. Re-graded: C1 swkbd text corruption (Critical), C2 audout queue degradation after pause (Critical), I1 pck candidates narrower than ProjectSettings/spec (Important), I2 MainLoop focus notifications missing (Important) → fix pass. M1-M10 deferred.
Final: Ruling: libnx focus-handling mode (SuspendHomeSleep default vs NoSuspend) left at default — a reasonable player gets a frozen-then-resumed game on Home/sleep, which the hardware session will show; switching modes is a one-call change — cost if wrong: sleep/resume behaves as process freeze, audio pause path unexercised.
Final: Ruling: localtime_r/%z correctness under newlib not verified — affects OS.get_datetime(false) only — cost if wrong: wrong local time in-game.
Final: Ruling: separate-thread render model with resize() unverified — spec §4.4 defers; thread_model stays safe — cost if wrong: none until that model is enabled.
Final: Ruling: single sideways Joy-Con (SL/SR) not mapped — spec lists dual Joy-Con and Pro only — cost if wrong: a player holding one Joy-Con sideways gets no SL/SR.
Final: Ruling: application/config/use_custom_user_dir ignored — by spec §4.7 design (one game per folder) — cost if wrong: none.
Final: Ruling: text_changed not emitted after swkbd commit — same as the Web platform — cost if wrong: a LineEdit listening only to text_changed misses the keyboard result.
Final: Ruling: OS::get_cwd not overridden — only used in Godot's own error text — cost if wrong: cosmetic.
Final: Ruling: Horizon exit-request timeout with auto_accept_quit=false is game-side — cost if wrong: none for the port.
Final: Ruling: proof-scene AudioStreamGenerator assumes 48 kHz — test project only — cost if wrong: none.
Final: minor (deferred): M1 comment claims the applet hook runs on a libnx thread; appletProcessMessage runs hooks on the calling (engine) thread.
Final: minor (deferred): M2 no host test asserts the resize/focus flag path.
Final: minor (deferred): M3 audoutExit guard uses audoutGetSampleRate()!=0; replace with an explicit service_open flag.
Final: minor (deferred): M4 finger_id % 16 slotting can alias a held finger after 15 taps; zero-state early return leaves touches pressed.
Final: minor (deferred): M5 the .nro is a post-action, not a SCons node (no rebuild when deleted, no clean).
Final: minor (deferred): M6 debug_symbols=yes not enforced or documented for the template.
Final: minor (deferred): M7 README nxlink note should say where the pck must sit relative to the uploaded NRO.
Final: minor (deferred): M8 is_user_dir_writable/set_user_dir_writable unused.
Final: minor (deferred): M9 switch_wrapper.h rename trick has no guard against a stray <switch.h> include.
Final: minor (deferred): M10 two-flag focus state could be one SafeNumeric.
Final: minor (deferred): R5 keep the thirdparty/engine patch list in one place for the next 4.7.x rebase.
Final: fixed I1 pck candidates — test "[SwitchHelpers] pck candidates mirror ProjectSettings and add cwd/game.pck" + dedupe case RED→GREEN, suite 8/8 doctest + 4/4 pytest; main() passes the hit as --main-pack.
Final: fixed C1 swkbd text corruption — no host-testable surface (display server); backspace loop removed, verified by build; hardware step pending. Ruling: fix without a failing test — cost if wrong: keyboard result wrong on device, caught in the hardware session.
Final: fixed C2 audout drain — no host-testable surface (libnx thread loop); drain loop added, verified by build; hardware sleep/resume step pending. Ruling: fix without a failing test — cost if wrong: crackle after resume, caught in the hardware session.
Final: fixed I2 MainLoop focus notifications — no host-testable surface; verified by build. Ruling as above.
Post-review: Ruling: Eden writes argv as 'homebrew <program_args>' (verified in Eden's nro.cpp on 2026-10-13), so executable_path() now falls back when argv[0] has no '/', and main() honours an explicit --main-pack — tests '[SwitchHelpers] explicit --main-pack argument wins' + Eden argv0 case RED→GREEN, suite 9/9 doctest — cost if wrong: pck not found on emulators, caught by the on-screen message.
Post-review: Ruling: first Eden run showed cwd '/' (no device); sd_cwd() maps device-less paths to sdmc: and the missing-pck screen now lists argv and whether sdmc:/ opens — test '[SwitchHelpers] working directory without a device maps to the SD card' RED→GREEN, suite 10/10 doctest — cost if wrong: wrong fallback folder, visible on the screen.
Post-review: Ruling: disable_path_overrides=False on switch (the template injects --main-pack; Godot rejects it otherwise) — pytest RED→GREEN; IPSwitch no-op registered so IP::create() has an implementation (networking out of scope) — cost if wrong: command-line path flags usable on homebrew, which is harmless.
