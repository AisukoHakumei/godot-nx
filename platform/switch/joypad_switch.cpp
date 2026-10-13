/**************************************************************************/
/*  joypad_switch.cpp                                                     */
/**************************************************************************/

#include "joypad_switch.h"

#include "display_server_switch.h"
#include "switch_helpers.h"
#include "switch_wrapper.h"

#include "core/input/input_event.h"
#include "core/os/os.h"

struct JoypadSwitch::Impl {
	PadState pad;
	HidVibrationDeviceHandle vibration_handles[2];
	int vibration_handle_count = 0;
};

namespace {

struct ButtonMap {
	uint64_t nx;
	JoyButton godot;
};

// Physical positions: Godot's layout is Xbox-style, so the Switch A (right)
// maps to JoyButton::B and so on. Godot's own Nintendo-layout handling swaps
// them back for display.
const ButtonMap BUTTON_MAP[] = {
	{ HidNpadButton_A, JoyButton::B },
	{ HidNpadButton_B, JoyButton::A },
	{ HidNpadButton_X, JoyButton::Y },
	{ HidNpadButton_Y, JoyButton::X },
	{ HidNpadButton_L, JoyButton::LEFT_SHOULDER },
	{ HidNpadButton_R, JoyButton::RIGHT_SHOULDER },
	{ HidNpadButton_Plus, JoyButton::START },
	{ HidNpadButton_Minus, JoyButton::BACK },
	{ HidNpadButton_StickL, JoyButton::LEFT_STICK },
	{ HidNpadButton_StickR, JoyButton::RIGHT_STICK },
	{ HidNpadButton_Up, JoyButton::DPAD_UP },
	{ HidNpadButton_Down, JoyButton::DPAD_DOWN },
	{ HidNpadButton_Left, JoyButton::DPAD_LEFT },
	{ HidNpadButton_Right, JoyButton::DPAD_RIGHT },
};

} // namespace

JoypadSwitch::JoypadSwitch(Input *p_input) {
	input = p_input;
	impl = memnew(Impl);

	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&impl->pad);
	hidInitializeTouchScreen();

	// Empty GUID: buttons are already delivered in Godot's logical layout, so
	// no gamepad-database remap must be applied on top.
	input->joy_connection_changed(DEVICE_ID, true, "Nintendo Switch Controller", "");
}

JoypadSwitch::~JoypadSwitch() {
	if (vibrating) {
		_set_vibration(0, 0);
	}
	input->joy_connection_changed(DEVICE_ID, false, "", "");
	memdelete(impl);
}

void JoypadSwitch::process() {
	padUpdate(&impl->pad);
	_process_buttons();
	_process_sticks();
	_process_touch();
	_process_vibration();
}

void JoypadSwitch::_process_buttons() {
	uint64_t buttons = padGetButtons(&impl->pad);
	uint64_t changed = buttons ^ buttons_prev;
	if (changed == 0) {
		return;
	}
	for (const ButtonMap &m : BUTTON_MAP) {
		if (changed & m.nx) {
			input->joy_button(DEVICE_ID, m.godot, (buttons & m.nx) != 0);
		}
	}
	buttons_prev = buttons;
}

void JoypadSwitch::_process_sticks() {
	HidAnalogStickState left = padGetStickPos(&impl->pad, 0);
	HidAnalogStickState right = padGetStickPos(&impl->pad, 1);
	uint64_t buttons = padGetButtons(&impl->pad);

	// libnx reports +Y as up; Godot expects +Y as down.
	float axes[6] = {
		SwitchHelpers::stick_axis(left.x),
		-SwitchHelpers::stick_axis(left.y),
		SwitchHelpers::stick_axis(right.x),
		-SwitchHelpers::stick_axis(right.y),
		(buttons & HidNpadButton_ZL) ? 1.0f : 0.0f,
		(buttons & HidNpadButton_ZR) ? 1.0f : 0.0f,
	};
	for (int i = 0; i < 6; i++) {
		if (axes[i] != axes_prev[i]) {
			input->joy_axis(DEVICE_ID, (JoyAxis)i, axes[i]);
			axes_prev[i] = axes[i];
		}
	}
}

void JoypadSwitch::_process_touch() {
	HidTouchScreenState state = {};
	if (hidGetTouchScreenStates(&state, 1) == 0) {
		return;
	}

	Size2i window = DisplayServer::get_singleton()->window_get_size();
	bool seen[MAX_TOUCHES] = {};

	for (int i = 0; i < state.count && i < MAX_TOUCHES; i++) {
		const HidTouchState &t = state.touches[i];
		int id = t.finger_id % MAX_TOUCHES;
		seen[id] = true;
		Vector2 pos = SwitchHelpers::touch_to_window(t.x, t.y, window);

		if (!touch_active[id]) {
			Ref<InputEventScreenTouch> ev;
			ev.instantiate();
			ev->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
			ev->set_index(id);
			ev->set_position(pos);
			ev->set_pressed(true);
			input->parse_input_event(ev);
			touch_active[id] = true;
		} else if (pos != touch_pos[id]) {
			Ref<InputEventScreenDrag> ev;
			ev.instantiate();
			ev->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
			ev->set_index(id);
			ev->set_position(pos);
			ev->set_relative(pos - touch_pos[id]);
			input->parse_input_event(ev);
		}
		touch_pos[id] = pos;
	}

	for (int id = 0; id < MAX_TOUCHES; id++) {
		if (touch_active[id] && !seen[id]) {
			Ref<InputEventScreenTouch> ev;
			ev.instantiate();
			ev->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
			ev->set_index(id);
			ev->set_position(touch_pos[id]);
			ev->set_pressed(false);
			input->parse_input_event(ev);
			touch_active[id] = false;
		}
	}
}

void JoypadSwitch::_set_vibration(float p_weak, float p_strong) {
	// Re-acquire motor handles whenever the controller style changes.
	uint32_t style = padGetStyleSet(&impl->pad);
	if (style != vibration_style || impl->vibration_handle_count == 0) {
		HidNpadIdType id = padIsHandheld(&impl->pad) ? HidNpadIdType_Handheld : HidNpadIdType_No1;
		HidNpadStyleTag tag = padIsHandheld(&impl->pad) ? HidNpadStyleTag_NpadHandheld : ((style & HidNpadStyleTag_NpadFullKey) ? HidNpadStyleTag_NpadFullKey : HidNpadStyleTag_NpadJoyDual);
		Result rc = hidInitializeVibrationDevices(impl->vibration_handles, 2, id, tag);
		impl->vibration_handle_count = R_SUCCEEDED(rc) ? 2 : 0;
		vibration_style = style;
		if (R_FAILED(rc)) {
			print_verbose("hidInitializeVibrationDevices failed; rumble disabled.");
			return;
		}
	}

	HidVibrationValue value;
	value.amp_low = CLAMP(p_weak, 0.0f, 1.0f);
	value.freq_low = 160.0f;
	value.amp_high = CLAMP(p_strong, 0.0f, 1.0f);
	value.freq_high = 320.0f;
	HidVibrationValue values[2] = { value, value };
	hidSendVibrationValues(impl->vibration_handles, values, impl->vibration_handle_count);
	vibrating = (value.amp_low > 0.0f || value.amp_high > 0.0f);
}

void JoypadSwitch::_process_vibration() {
	uint64_t timestamp = input->get_joy_vibration_timestamp(DEVICE_ID);
	if (timestamp != vibration_timestamp) {
		vibration_timestamp = timestamp;
		Vector2 strength = input->get_joy_vibration_strength(DEVICE_ID);
		float duration = input->get_joy_vibration_duration(DEVICE_ID);
		_set_vibration(strength.x, strength.y);
		vibration_stop_usec = (vibrating && duration > 0.0f) ? OS::get_singleton()->get_ticks_usec() + uint64_t(duration * 1000000.0f) : 0;
	} else if (vibrating && vibration_stop_usec != 0 && OS::get_singleton()->get_ticks_usec() >= vibration_stop_usec) {
		_set_vibration(0, 0);
		vibration_stop_usec = 0;
	}
}
