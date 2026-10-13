# Switch packaging (phase 1)

`package.py` turns the release template and an exported `.pck` into a folder
ready for the SD card.

```
python platform/switch/tools/package.py \
    --pck path/to/game.pck --name mygame --author "Me" --version 1.0.0 \
    [--icon icon.jpg] [--elf bin/godot.switch.template_release.arm64.elf] \
    --out ./sdmc
```

Result: `sdmc/switch/mygame/mygame.nro` and `sdmc/switch/mygame/mygame.pck`.
Copy the `switch/` folder to the root of the SD card.

## Launching

A 3D game needs the full application memory pool. Launch the NRO through
**title override**: in the homebrew menu, hold **R** while opening any
installed title, then pick `mygame.nro`. A plain album/hbmenu launch runs in
applet mode; the template detects that and shows a message instead of
starting.

For development, `nxlink -s sdmc/switch/mygame/mygame.nro` uploads the NRO
to a console running hbmenu's netloader and streams stdout back.

## Inputs

- `--elf`: the export template built with `scons platform=switch target=template_release`.
- `--icon`: a 256x256 JPEG; the Godot logo is used by default.
- `--pck`: export from the Godot editor with the texture formats set to
  S3TC/BPTC only (`texture_format/s3tc_bptc = true`, `etc2_astc = false`).
