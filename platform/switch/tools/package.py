#!/usr/bin/env python3
"""Phase-1 packaging for the Switch port.

Builds an SD-card folder from the release template ELF and an exported .pck:

    switch/<name>/<name>.nro   (template + NACP metadata + icon, via nacptool and elf2nro)
    switch/<name>/<name>.pck   (copied from --pck)

Copy the resulting `switch/` folder to the root of the SD card and launch the
NRO via title override (hold R while opening an installed title) so the game
runs in application mode. See README.md next to this script.
"""

import argparse
import os
import shutil
import subprocess
import sys

DEFAULT_ELF = os.path.join("bin", "godot.switch.template_release.arm64.elf")
DEFAULT_ICON = os.path.join("platform", "switch", "tools", "default_icon.jpg")


def parse_args(argv):
    parser = argparse.ArgumentParser(description="Package a Godot .pck with the Switch template into an SD-card folder.")
    parser.add_argument("--pck", required=True, help="Exported project .pck")
    parser.add_argument("--name", required=True, help="Game folder and NRO name, also the NACP title")
    parser.add_argument("--author", required=True, help="NACP author")
    parser.add_argument("--version", required=True, help="NACP version string, e.g. 1.0.0")
    parser.add_argument("--icon", default=DEFAULT_ICON, help="256x256 JPEG icon (default: Godot logo)")
    parser.add_argument("--elf", default=DEFAULT_ELF, help="Template ELF (default: release template in bin/)")
    parser.add_argument("--out", required=True, help="Output root; the switch/<name>/ folder is created inside it")
    return parser.parse_args(argv)


def output_layout(out, name):
    """Paths of everything the package produces under <out>/switch/<name>/."""
    game_dir = os.path.join(out, "switch", name)
    return {
        "dir": game_dir,
        "nro": os.path.join(game_dir, name + ".nro"),
        "pck": os.path.join(game_dir, name + ".pck"),
        "nacp": os.path.join(game_dir, name + ".nacp"),
    }


def plan_commands(args, devkitpro):
    """The exact nacptool and elf2nro invocations, in order."""
    tools = os.path.join(devkitpro, "tools", "bin")
    layout = output_layout(args.out, args.name)
    nacptool = [os.path.join(tools, "nacptool"), "--create", args.name, args.author, args.version, layout["nacp"]]
    elf2nro = [
        os.path.join(tools, "elf2nro"),
        args.elf,
        layout["nro"],
        "--icon=" + args.icon,
        "--nacp=" + layout["nacp"],
    ]
    return [nacptool, elf2nro]


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    devkitpro = os.environ.get("DEVKITPRO")
    if not devkitpro:
        print("DEVKITPRO is not set.", file=sys.stderr)
        return 2
    for required in (args.pck, args.elf, args.icon):
        if not os.path.isfile(required):
            print(f"Missing input: {required}", file=sys.stderr)
            return 2

    layout = output_layout(args.out, args.name)
    os.makedirs(layout["dir"], exist_ok=True)
    for cmd in plan_commands(args, devkitpro):
        print(" ".join(cmd))
        subprocess.run(cmd, check=True)
    shutil.copyfile(args.pck, layout["pck"])
    os.remove(layout["nacp"])
    print(f"Packaged {layout['nro']} and {layout['pck']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
