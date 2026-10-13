/**************************************************************************/
/*  test_switch_helpers.cpp                                               */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_switch_helpers)

#include "platform/switch/switch_helpers.h"

namespace TestSwitchHelpers {

TEST_CASE("[SwitchHelpers] window size follows dock state") {
	CHECK(SwitchHelpers::window_size_for_docked(true) == Size2i(1920, 1080));
	CHECK(SwitchHelpers::window_size_for_docked(false) == Size2i(1280, 720));
}

TEST_CASE("[SwitchHelpers] executable path falls back when argv0 is empty") {
	CHECK(SwitchHelpers::executable_path("", "sdmc:/") == "sdmc:/game.nro");
	CHECK(SwitchHelpers::executable_path("sdmc:/switch/mygame/mygame.nro", "sdmc:/") == "sdmc:/switch/mygame/mygame.nro");
	// Eden passes "homebrew" as argv[0]; anything without a directory falls back too.
	CHECK(SwitchHelpers::executable_path("homebrew", "sdmc:/") == "sdmc:/game.nro");
	CHECK(SwitchHelpers::user_dir_for_executable("sdmc:/switch/mygame/mygame.nro") == "sdmc:/switch/mygame/user");
}

TEST_CASE("[SwitchHelpers] pck candidates mirror ProjectSettings and add cwd/game.pck") {
	Vector<String> c = SwitchHelpers::pck_candidates("sdmc:/switch/mygame/mygame.nro", "sdmc:/switch");
	CHECK(c.size() == 5);
	CHECK(c[0] == "sdmc:/switch/mygame/mygame.pck");
	CHECK(c[1] == "sdmc:/switch/mygame/mygame.nro.pck");
	CHECK(c[2] == "sdmc:/switch/mygame.pck");
	CHECK(c[3] == "sdmc:/switch/mygame.nro.pck");
	CHECK(c[4] == "sdmc:/switch/game.pck");
}

TEST_CASE("[SwitchHelpers] pck candidates deduplicate when cwd is the NRO folder") {
	Vector<String> c = SwitchHelpers::pck_candidates("sdmc:/switch/mygame/mygame.nro", "sdmc:/switch/mygame");
	CHECK(c.size() == 3);
	CHECK(c[2] == "sdmc:/switch/mygame/game.pck");
}

TEST_CASE("[SwitchHelpers] touch scales to docked window without overflow") {
	CHECK(SwitchHelpers::touch_to_window(1279, 719, Size2i(1920, 1080)).is_equal_approx(Vector2(1918.5f, 1078.5f)));
	CHECK(SwitchHelpers::touch_to_window(0, 0, Size2i(1280, 720)) == Vector2(0, 0));
}

TEST_CASE("[SwitchHelpers] stick and sample conversion") {
	CHECK(SwitchHelpers::stick_axis(32767) == doctest::Approx(1.0f));
	CHECK(SwitchHelpers::stick_axis(-40000) == doctest::Approx(-1.0f));
	int32_t src[2] = { 0x7FFF0000, (int32_t)0x80000000 };
	int16_t dst[2];
	SwitchHelpers::mix_to_int16(src, dst, 2);
	CHECK(dst[0] == 32767);
	CHECK(dst[1] == -32768);
}

TEST_CASE("[SwitchHelpers] missing pck message lists every candidate") {
	String msg = SwitchHelpers::missing_pck_message("sdmc:/switch/mygame/mygame.nro", "sdmc:/");
	CHECK(msg.contains("sdmc:/switch/mygame/mygame.pck"));
	CHECK(msg.contains("sdmc:/switch/mygame/mygame.nro.pck"));
	CHECK(msg.contains("sdmc:/game.pck"));
	CHECK(msg.begins_with("No game data found."));
}

TEST_CASE("[SwitchHelpers] audout buffer sizing") {
	CHECK(SwitchHelpers::audout_buffer_bytes(480) == 1920);
	CHECK(SwitchHelpers::audout_block_bytes() == 4096);
}

TEST_CASE("[SwitchHelpers] explicit --main-pack argument wins") {
	const char *argv1[] = { "--verbose", "--main-pack", "sdmc:/switch/x/x.pck", "--fixed-fps", "30" };
	CHECK(SwitchHelpers::main_pack_from_args(5, argv1) == "sdmc:/switch/x/x.pck");
	const char *argv2[] = { "--verbose", "--main-pack" };
	CHECK(SwitchHelpers::main_pack_from_args(2, argv2) == "");
	CHECK(SwitchHelpers::main_pack_from_args(0, nullptr) == "");
}

} // namespace TestSwitchHelpers
