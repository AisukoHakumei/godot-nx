# Project data loading and filesystem paths

Type: grilling
Status: open
Blocked by: 08
Map: ../map.md

## Question

How does the template find its `.pck`: `argv[0]`-relative on `sdmc:/switch/<game>/<game>.pck`, a fixed name beside the NRO, or `--main-pack`? How are `argv` passed under hbmenu and under `nxlink`? Where do `user://`, cache, and config map (`sdmc:/switch/<game>/user/`)? Is the romfs mounted at all in the first milestone? What does `OS::get_executable_path` return?

Context: Stary2001's 3.x port used an hbmenu `fileassoc` config plus a forwarder NRO to launch a `.pck`; Ozuba and Stary both map `user://` to `sdmc:/switch/godot/app_userdata/<name>` (see [research/03-prior-art.md](../research/03-prior-art.md)).
Context: Eden passes libnx argv only when launched with a non-empty `-p`; Ryubing passes none. The `.pck` discovery must not depend solely on argv; see [research/07-emulator.md](../research/07-emulator.md).
