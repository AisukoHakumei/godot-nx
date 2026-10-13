/**************************************************************************/
/*  joypad_switch.h                                                       */
/**************************************************************************/

#pragma once

#include "core/input/input.h"

// Polls libnx HID each frame: one merged pad (handheld, dual Joy-Con or Pro
// Controller) as Godot joypad 0, the touch screen, and rumble requests.
class JoypadSwitch {
public:
	static constexpr int DEVICE_ID = 0;
	static constexpr int MAX_TOUCHES = 16;

private:
	struct Impl;
	Impl *impl = nullptr;
	Input *input = nullptr;

	uint64_t buttons_prev = 0;
	float axes_prev[6] = { 0, 0, 0, 0, 0, 0 };

	uint32_t touch_style_prev = 0;
	bool touch_active[MAX_TOUCHES] = {};
	Vector2 touch_pos[MAX_TOUCHES];

	uint64_t vibration_timestamp = 0;
	uint64_t vibration_stop_usec = 0;
	bool vibrating = false;
	uint32_t vibration_style = 0;

	void _process_buttons();
	void _process_sticks();
	void _process_touch();
	void _process_vibration();
	void _set_vibration(float p_weak, float p_strong);

public:
	void process();

	JoypadSwitch(Input *p_input);
	~JoypadSwitch();
};
