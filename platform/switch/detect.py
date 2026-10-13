import os
import sys
from typing import TYPE_CHECKING

from methods import print_error
from platform_methods import validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment


def get_name():
    return "Switch"


def devkitpro_root(environ):
    """Return the DEVKITPRO root when it holds a devkitA64 compiler, else None."""
    root = environ.get("DEVKITPRO")
    if not root:
        return None
    gcc = os.path.join(root, "devkitA64", "bin", "aarch64-none-elf-gcc")
    if not os.path.isfile(gcc):
        return None
    return root


def can_build():
    return devkitpro_root(os.environ) is not None


def get_tools(env: "SConsEnvironment"):
    return ["cc", "c++", "ar", "link", "textfile"]


def get_opts():
    return []


def get_doc_classes():
    return []


def get_doc_path():
    return "doc_classes"


def get_flags():
    return {
        "arch": "arm64",
        "target": "template_release",
        "opengl3": True,
        "vulkan": False,
        "use_volk": False,
        "use_sowrap": False,
        "module_mono_enabled": False,
        "module_openxr_enabled": False,
        "module_webxr_enabled": False,
        "module_camera_enabled": False,
        "module_enet_enabled": False,
        "module_upnp_enabled": False,
        "module_webrtc_enabled": False,
        "module_websocket_enabled": False,
        "module_mobile_vr_enabled": False,
    }


def configure(env: "SConsEnvironment"):
    validate_arch(env["arch"], get_name(), ["arm64"])

    if env.editor_build:
        print_error("The Switch platform only builds export templates (target=template_release or template_debug).")
        sys.exit(255)

    root = devkitpro_root(os.environ)
    if root is None:
        print_error("DEVKITPRO is not set or has no devkitA64 compiler.")
        sys.exit(255)

    bindir = os.path.join(root, "devkitA64", "bin")
    env["CC"] = os.path.join(bindir, "aarch64-none-elf-gcc")
    env["CXX"] = os.path.join(bindir, "aarch64-none-elf-g++")
    env["LINK"] = os.path.join(bindir, "aarch64-none-elf-g++")
    env["AR"] = os.path.join(bindir, "aarch64-none-elf-gcc-ar")
    env["RANLIB"] = os.path.join(bindir, "aarch64-none-elf-gcc-ranlib")
    env["STRIP"] = os.path.join(bindir, "aarch64-none-elf-strip")
    env["PROGSUFFIX"] = ".elf"
    env["DEVKITPRO"] = root

    env.Append(
        CCFLAGS=[
            "-march=armv8-a+crc+crypto",
            "-mtune=cortex-a57",
            "-mtp=soft",
            "-fPIE",
            "-ffunction-sections",
            "-fdata-sections",
        ]
    )
    env.Append(
        LINKFLAGS=[
            "-specs=" + os.path.join(root, "libnx", "switch.specs"),
            "-fPIE",
            "-Wl,--gc-sections",
        ]
    )
    env.Append(
        CPPDEFINES=[
            "__SWITCH__",
            "SWITCH_ENABLED",
            "HORIZON_ENABLED",
            "GLES3_ENABLED",
            "GLES_API_ENABLED",
            "PLATFORM_THREAD_OVERRIDE",
        ]
    )
    env.Prepend(CPPPATH=["#platform/switch"])
    env.Prepend(
        CPPPATH=[
            os.path.join(root, "libnx", "include"),
            os.path.join(root, "portlibs", "switch", "include"),
        ]
    )
    env.Append(
        LIBPATH=[
            os.path.join(root, "libnx", "lib"),
            os.path.join(root, "portlibs", "switch", "lib"),
        ]
    )
    env.Append(LIBS=["EGL", "GLESv2", "glapi", "drm_nouveau", "nx", "m"])
