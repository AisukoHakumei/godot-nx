import importlib.util
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def _load():
    if REPO_ROOT not in sys.path:
        sys.path.insert(0, REPO_ROOT)
    path = os.path.join(REPO_ROOT, "platform", "switch", "tools", "package.py")
    spec = importlib.util.spec_from_file_location("switch_package", path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def test_output_layout():
    m = _load()
    lay = m.output_layout("/o", "mygame")
    assert lay == {
        "dir": "/o/switch/mygame",
        "nro": "/o/switch/mygame/mygame.nro",
        "pck": "/o/switch/mygame/mygame.pck",
        "nacp": "/o/switch/mygame/mygame.nacp",
    }


def test_plan_commands_uses_nacptool_then_elf2nro():
    m = _load()
    ns = m.parse_args(["--pck", "g.pck", "--name", "mygame", "--author", "Me", "--version", "1.0.0", "--out", "/o"])
    cmds = m.plan_commands(ns, "/opt/devkitpro")
    assert cmds[0] == ["/opt/devkitpro/tools/bin/nacptool", "--create", "mygame", "Me", "1.0.0", "/o/switch/mygame/mygame.nacp"]
    assert cmds[1] == [
        "/opt/devkitpro/tools/bin/elf2nro",
        "bin/godot.switch.template_release.arm64.elf",
        "/o/switch/mygame/mygame.nro",
        "--icon=platform/switch/tools/default_icon.jpg",
        "--nacp=/o/switch/mygame/mygame.nacp",
    ]
