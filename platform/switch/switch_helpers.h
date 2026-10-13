/**************************************************************************/
/*  switch_helpers.h                                                      */
/**************************************************************************/

#pragma once

// Pure helpers for the Switch platform layer. This header must stay free of
// libnx includes so the host test build (tests/platform/switch) can compile it.

#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

#include <cstring>

namespace SwitchHelpers {

// Native touch panel resolution reported by libnx regardless of dock state.
static constexpr int TOUCH_WIDTH = 1280;
static constexpr int TOUCH_HEIGHT = 720;

static inline Size2i window_size_for_docked(bool p_docked) {
	return p_docked ? Size2i(1920, 1080) : Size2i(1280, 720);
}

// newlib reports the working directory without a device under some loaders
// (Eden gives "/"); a path with no device is on the SD card, libnx's default.
static inline String sd_cwd(const String &p_cwd) {
	if (p_cwd.is_empty()) {
		return "sdmc:/";
	}
	if (p_cwd.contains(":")) {
		return p_cwd;
	}
	return "sdmc:" + (p_cwd.begins_with("/") ? p_cwd : "/" + p_cwd);
}

static inline String fallback_executable_path(const String &p_cwd) {
	return p_cwd.path_join("game.nro");
}

// argv[0] is the NRO path under hbmenu/nxlink, empty under some emulators,
// and the literal "homebrew" under Eden; only a value with a directory is usable.
static inline String executable_path(const String &p_argv0, const String &p_cwd) {
	if (p_argv0.is_empty() || !p_argv0.contains("/")) {
		return fallback_executable_path(p_cwd);
	}
	return p_argv0;
}

// The value following an explicit "--main-pack" in the arguments after argv[0],
// or empty. Lets an emulator launch name the pck directly.
static inline String main_pack_from_args(int p_argc, const char **p_argv) {
	for (int i = 0; i + 1 < p_argc; i++) {
		if (p_argv[i] && strcmp(p_argv[i], "--main-pack") == 0 && p_argv[i + 1]) {
			return String::utf8(p_argv[i + 1]);
		}
	}
	return String();
}

static inline String user_dir_for_executable(const String &p_exec_path) {
	return p_exec_path.get_base_dir().path_join("user");
}

// Where the template looks for project data, in order: the names
// ProjectSettings::_setup tries beside the executable, the same names in the
// working directory, then a fixed game.pck in the working directory. The first
// hit is passed to the engine as --main-pack so the search here is the truth.
static inline Vector<String> pck_candidates(const String &p_exec_path, const String &p_cwd) {
	Vector<String> out;
	const String exec_dir = p_exec_path.get_base_dir();
	const String base = p_exec_path.get_file().get_basename();
	const String file = p_exec_path.get_file();
	const String names[] = {
		exec_dir.path_join(base + ".pck"),
		exec_dir.path_join(file + ".pck"),
		p_cwd.path_join(base + ".pck"),
		p_cwd.path_join(file + ".pck"),
		p_cwd.path_join("game.pck"),
	};
	for (const String &n : names) {
		if (!out.has(n)) {
			out.push_back(n);
		}
	}
	return out;
}

// Text for the pre-renderer screen shown when no project data is found.
static inline String missing_pck_message(const String &p_exec_path, const String &p_cwd) {
	String msg = "No game data found. Looked for:";
	for (const String &c : pck_candidates(p_exec_path, p_cwd)) {
		msg += "\n" + c;
	}
	return msg;
}

static inline Vector2 touch_to_window(int p_x, int p_y, const Size2i &p_window) {
	return Vector2(
			float(p_x) * float(p_window.x) / float(TOUCH_WIDTH),
			float(p_y) * float(p_window.y) / float(TOUCH_HEIGHT));
}

static inline float stick_axis(int32_t p_raw) {
	return CLAMP(float(p_raw) / 32767.0f, -1.0f, 1.0f);
}

// audout: 48 kHz stereo int16. 480 frames = 10 ms = 1920 bytes, held in one
// 4096-byte block aligned to 0x1000 as the service requires.
static constexpr int AUDOUT_CHANNELS = 2;
static constexpr int AUDOUT_BYTES_PER_SAMPLE = 2;

static constexpr int audout_buffer_bytes(int p_frames) {
	return p_frames * AUDOUT_CHANNELS * AUDOUT_BYTES_PER_SAMPLE;
}

static constexpr int audout_block_bytes() {
	return 4096;
}

// AudioServer hands out int32 frames whose top 16 bits carry the sample.
static inline void mix_to_int16(const int32_t *p_src, int16_t *p_dst, int p_samples) {
	for (int i = 0; i < p_samples; i++) {
		p_dst[i] = int16_t(p_src[i] >> 16);
	}
}

} // namespace SwitchHelpers
