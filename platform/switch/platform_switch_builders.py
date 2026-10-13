"""Post-build step: turn the template ELF into an NRO with nacptool + elf2nro."""

import os
import subprocess

import methods


def make_nro(target, source, env):
    elf = str(target[0])
    stem = elf[:-4] if elf.endswith(".elf") else elf
    nro = stem + ".nro"
    nacp = stem + ".nacp"
    tools = os.path.join(env["DEVKITPRO"], "tools", "bin")
    v = methods.get_version_info(silent=True)
    version = f"{v['major']}.{v['minor']}.{v['patch']}"
    subprocess.run([os.path.join(tools, "nacptool"), "--create", "Godot", "Godot Engine", version, nacp], check=True)
    cmd = [os.path.join(tools, "elf2nro"), elf, nro, "--nacp=" + nacp]
    icon = os.path.join("platform", "switch", "tools", "default_icon.jpg")
    if os.path.isfile(icon):
        cmd.append("--icon=" + icon)
    subprocess.run(cmd, check=True)
