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

namespace SwitchHelpers {

// Native touch panel resolution reported by libnx regardless of dock state.
static constexpr int TOUCH_WIDTH = 1280;
static constexpr int TOUCH_HEIGHT = 720;

static inline Size2i window_size_for_docked(bool p_docked) {
	return p_docked ? Size2i(1920, 1080) : Size2i(1280, 720);
}

static inline String fallback_executable_path(const String &p_cwd) {
	return p_cwd.path_join("game.nro");
}

static inline String executable_path(const String &p_argv0, const String &p_cwd) {
	return p_argv0.is_empty() ? fallback_executable_path(p_cwd) : p_argv0;
}

static inline String user_dir_for_executable(const String &p_exec_path) {
	return p_exec_path.get_base_dir().path_join("user");
}

// The names ProjectSettings::_setup tries next to the executable, in order.
static inline Vector<String> pck_candidates(const String &p_exec_path) {
	Vector<String> out;
	out.push_back(p_exec_path.get_basename() + ".pck");
	out.push_back(p_exec_path + ".pck");
	return out;
}

// Text for the pre-renderer screen shown when no project data is found.
static inline String missing_pck_message(const String &p_exec_path) {
	String msg = "No game data found. Looked for:";
	for (const String &c : pck_candidates(p_exec_path)) {
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
