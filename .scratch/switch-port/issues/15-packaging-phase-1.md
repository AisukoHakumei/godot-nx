# Packaging phase 1: elf2nro script and metadata

Type: grilling
Status: open
Blocked by: 13
Map: ../map.md

## Question

What does the phase-1 packaging script do: `nacptool` fields (title, author, version), icon size/format, `elf2nro` invocation, output layout on the SD card (`switch/<game>/<game>.nro` + `.pck`), and where the script lives (`platform/switch/tools/`)? What stays for the later editor export preset?

Context: Stary2001's exporter rewrites the NRO `ASET` section (icon/NACP/RomFS) in C++ via `nro.h`/`nacp.h`; consider whether phase 1 reuses that or stays with `nacptool` + `elf2nro` (see [research/03-prior-art.md](../research/03-prior-art.md)).
