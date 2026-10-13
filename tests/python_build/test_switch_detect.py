import importlib.util
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def _load():
    if REPO_ROOT not in sys.path:
        sys.path.insert(0, REPO_ROOT)
    spec = importlib.util.spec_from_file_location("switch_detect", os.path.join(REPO_ROOT, "platform", "switch", "detect.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def test_devkitpro_root_requires_compiler(tmp_path):
    m = _load()
    assert m.devkitpro_root({}) is None
    assert m.devkitpro_root({"DEVKITPRO": str(tmp_path)}) is None
    gcc = tmp_path / "devkitA64" / "bin" / "aarch64-none-elf-gcc"
    gcc.parent.mkdir(parents=True)
    gcc.write_text("")
    assert m.devkitpro_root({"DEVKITPRO": str(tmp_path)}) == str(tmp_path)


def test_flags_force_gles_and_arm64():
    f = _load().get_flags()
    assert f["arch"] == "arm64" and f["opengl3"] is True and f["vulkan"] is False
    assert f["use_volk"] is False and f["use_sowrap"] is False
    assert f["builtin_pcre2_with_jit"] is False
    assert f["disable_path_overrides"] is False  # the template injects --main-pack
    for mod in ["mono", "openxr", "webxr", "camera", "enet", "upnp", "webrtc", "websocket", "mobile_vr", "raycast"]:
        assert f[f"module_{mod}_enabled"] is False
