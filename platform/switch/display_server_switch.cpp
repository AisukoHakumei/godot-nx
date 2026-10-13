/**************************************************************************/
/*  display_server_switch.cpp                                             */
/**************************************************************************/

#include "display_server_switch.h"

#include "audio_driver_switch.h"
#include "switch_wrapper.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "drivers/gles3/rasterizer_gles3.h"

DisplayServerSwitch *DisplayServerSwitch::get_singleton() {
	return static_cast<DisplayServerSwitch *>(DisplayServer::get_singleton());
}

bool DisplayServerSwitch::_is_docked() const {
	return appletGetOperationMode() == AppletOperationMode_Console;
}

// Runs on libnx's applet message thread: only sets flags; process_events() acts on them.
static void _switch_applet_hook(AppletHookType p_hook, void *p_param) {
	DisplayServerSwitch *ds = static_cast<DisplayServerSwitch *>(p_param);
	switch (p_hook) {
		case AppletHookType_OnOperationMode:
			ds->set_resize_pending();
			break;
		case AppletHookType_OnFocusState:
			ds->set_focus_pending(appletGetFocusState() == AppletFocusState_InFocus);
			break;
		case AppletHookType_OnResume:
			ds->set_resume_pending();
			break;
		default:
			break;
	}
}

// Registration.

Vector<String> DisplayServerSwitch::get_rendering_drivers_func() {
	Vector<String> drivers;
	drivers.push_back("opengl3");
	return drivers;
}

DisplayServer *DisplayServerSwitch::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerSwitch(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, p_parent_window, r_error));
	if (r_error != OK) {
		memdelete(ds);
		ERR_PRINT("Unable to create the Switch display server.");
		return nullptr;
	}
	return ds;
}

void DisplayServerSwitch::register_switch_driver() {
	register_create_function("switch", create_func, get_rendering_drivers_func);
}

// Callbacks.

template <typename... Args>
void DisplayServerSwitch::_window_callback(const Callable &p_callable, bool p_deferred, const Args &...p_rest_args) const {
	if (p_callable.is_valid()) {
		if (p_deferred) {
			p_callable.call_deferred(p_rest_args...);
		} else {
			p_callable.call(p_rest_args...);
		}
	}
}

void DisplayServerSwitch::send_window_event(DisplayServerEnums::WindowEvent p_event, bool p_deferred) const {
	_window_callback(window_event_callback, p_deferred, int(p_event));
}

void DisplayServerSwitch::send_input_event(const Ref<InputEvent> &p_event) const {
	_window_callback(input_event_callback, false, p_event);
}

void DisplayServerSwitch::send_input_text(const String &p_text) const {
	_window_callback(input_text_callback, false, p_text, false);
}

void DisplayServerSwitch::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerSwitch::get_singleton()->send_input_event(p_event);
}

void DisplayServerSwitch::set_focus_pending(bool p_focused) {
	if (p_focused) {
		focus_pending_value.set();
	} else {
		focus_pending_value.clear();
	}
	focus_event_pending.set();
}

// Features and screens.

bool DisplayServerSwitch::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
		case DisplayServerEnums::FEATURE_VIRTUAL_KEYBOARD:
		case DisplayServerEnums::FEATURE_SWAP_BUFFERS:
			return true;
		default:
			return false;
	}
}

String DisplayServerSwitch::get_name() const {
	return "Switch";
}

int DisplayServerSwitch::get_screen_count() const {
	return 1;
}

int DisplayServerSwitch::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerSwitch::screen_get_position(int p_screen) const {
	return Point2i();
}

Size2i DisplayServerSwitch::screen_get_size(int p_screen) const {
	return gl_manager.get_window_size();
}

Rect2i DisplayServerSwitch::screen_get_usable_rect(int p_screen) const {
	return Rect2i(Point2i(), gl_manager.get_window_size());
}

int DisplayServerSwitch::screen_get_dpi(int p_screen) const {
	return 237; // 6.2" 1280x720 handheld panel.
}

float DisplayServerSwitch::screen_get_refresh_rate(int p_screen) const {
	return 60.0f;
}

// Windows: exactly one.

Vector<DisplayServerEnums::WindowID> DisplayServerSwitch::get_window_list() const {
	Vector<DisplayServerEnums::WindowID> ret;
	ret.push_back(DisplayServerEnums::MAIN_WINDOW_ID);
	return ret;
}

DisplayServerEnums::WindowID DisplayServerSwitch::get_window_at_screen_position(const Point2i &p_position) const {
	return DisplayServerEnums::MAIN_WINDOW_ID;
}

void DisplayServerSwitch::window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window) {
	window_attached_instance_id = p_instance;
}

ObjectID DisplayServerSwitch::window_get_attached_instance_id(DisplayServerEnums::WindowID p_window) const {
	return window_attached_instance_id;
}

void DisplayServerSwitch::window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	rect_changed_callback = p_callable;
}

void DisplayServerSwitch::window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerSwitch::window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerSwitch::window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerSwitch::window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
}

void DisplayServerSwitch::window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) {
}

int DisplayServerSwitch::window_get_current_screen(DisplayServerEnums::WindowID p_window) const {
	return 0;
}

void DisplayServerSwitch::window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window) {
}

Point2i DisplayServerSwitch::window_get_position(DisplayServerEnums::WindowID p_window) const {
	return Point2i();
}

Point2i DisplayServerSwitch::window_get_position_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return Point2i();
}

void DisplayServerSwitch::window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window) {
}

void DisplayServerSwitch::window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) {
}

void DisplayServerSwitch::window_set_max_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerSwitch::window_get_max_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i();
}

void DisplayServerSwitch::window_set_min_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerSwitch::window_get_min_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i();
}

void DisplayServerSwitch::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerSwitch::window_get_size(DisplayServerEnums::WindowID p_window) const {
	return gl_manager.get_window_size();
}

Size2i DisplayServerSwitch::window_get_size_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return gl_manager.get_window_size();
}

void DisplayServerSwitch::window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window) {
}

DisplayServerEnums::WindowMode DisplayServerSwitch::window_get_mode(DisplayServerEnums::WindowID p_window) const {
	return DisplayServerEnums::WINDOW_MODE_FULLSCREEN;
}

bool DisplayServerSwitch::window_is_maximize_allowed(DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerSwitch::window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window) {
}

bool DisplayServerSwitch::window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerSwitch::window_request_attention(DisplayServerEnums::WindowID p_window) {
}

void DisplayServerSwitch::window_move_to_foreground(DisplayServerEnums::WindowID p_window) {
}

bool DisplayServerSwitch::window_is_focused(DisplayServerEnums::WindowID p_window) const {
	return true;
}

bool DisplayServerSwitch::window_can_draw(DisplayServerEnums::WindowID p_window) const {
	return true;
}

bool DisplayServerSwitch::can_any_window_draw() const {
	return true;
}

void DisplayServerSwitch::window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window) {
	gl_manager.set_use_vsync(p_vsync_mode != DisplayServerEnums::VSYNC_DISABLED);
}

DisplayServerEnums::VSyncMode DisplayServerSwitch::window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const {
	return gl_manager.is_using_vsync() ? DisplayServerEnums::VSYNC_ENABLED : DisplayServerEnums::VSYNC_DISABLED;
}

// Frame loop.

void DisplayServerSwitch::process_events() {
	if (!appletMainLoop()) {
		send_window_event(DisplayServerEnums::WINDOW_EVENT_CLOSE_REQUEST);
	}

	if (resize_pending.is_set()) {
		resize_pending.clear();
		gl_manager.resize(_is_docked());
		Size2i size = gl_manager.get_window_size();
		print_verbose(vformat("Display changed, window is now %dx%d.", size.x, size.y));
		_window_callback(rect_changed_callback, false, Rect2i(Point2i(), size));
	}

	if (resume_pending.is_set()) {
		resume_pending.clear();
		print_verbose("Resumed from sleep, recreating the EGL surface.");
		gl_manager.resize(_is_docked());
	}

	if (focus_event_pending.is_set()) {
		focus_event_pending.clear();
		bool focused = focus_pending_value.is_set();
		print_verbose(focused ? "Focus gained." : "Focus lost.");
		if (AudioDriverSwitch::get_singleton()) {
			AudioDriverSwitch::get_singleton()->set_paused(!focused);
		}
		send_window_event(focused ? DisplayServerEnums::WINDOW_EVENT_FOCUS_IN : DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT);
	}

	if (joypad) {
		joypad->process();
	}

	Input::get_singleton()->flush_buffered_events();
}

// On-screen keyboard: the system swkbd applet, shown synchronously.

void DisplayServerSwitch::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, DisplayServerEnums::VirtualKeyboardType p_type, int p_max_length, int p_cursor_start, int p_cursor_end) {
	SwkbdConfig kbd;
	if (R_FAILED(swkbdCreate(&kbd, 0))) {
		WARN_PRINT("swkbdCreate failed; no on-screen keyboard.");
		return;
	}
	swkbdConfigMakePresetDefault(&kbd);
	CharString existing = p_existing_text.utf8();
	swkbdConfigSetInitialText(&kbd, existing.get_data());
	if (p_max_length > 0) {
		swkbdConfigSetStringLenMax(&kbd, p_max_length);
	}

	char out[2048] = {};
	Result rc = swkbdShow(&kbd, out, sizeof(out));
	swkbdClose(&kbd);
	if (R_FAILED(rc)) {
		return; // Cancelled.
	}

	// Replace the existing text: erase it, then commit the result.
	for (int i = 0; i < p_existing_text.length(); i++) {
		Ref<InputEventKey> down;
		down.instantiate();
		down->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
		down->set_keycode(Key::BACKSPACE);
		down->set_physical_keycode(Key::BACKSPACE);
		down->set_pressed(true);
		Input::get_singleton()->parse_input_event(down);
		Ref<InputEventKey> up = down->duplicate();
		up->set_pressed(false);
		Input::get_singleton()->parse_input_event(up);
	}
	send_input_text(String::utf8(out));
}

void DisplayServerSwitch::virtual_keyboard_hide() {
}

int DisplayServerSwitch::virtual_keyboard_get_height() const {
	return 0;
}

void DisplayServerSwitch::swap_buffers() {
	gl_manager.swap_buffers();
}

void DisplayServerSwitch::release_rendering_thread() {
	gl_manager.release_current();
}

// Lifecycle.

DisplayServerSwitch::DisplayServerSwitch(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	r_error = ERR_CANT_CREATE;

	gl_manager.set_use_vsync(p_vsync_mode != DisplayServerEnums::VSYNC_DISABLED);
	Error err = gl_manager.initialize(_is_docked());
	if (err != OK) {
		ERR_PRINT("EGL initialization failed; cannot create the Switch display server.");
		return;
	}
	gl_initialized = true;

	RasterizerGLES3::make_current(false);

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	joypad = memnew(JoypadSwitch(Input::get_singleton()));

	hook_cookie = memnew(AppletHookCookie);
	appletHook(hook_cookie, _switch_applet_hook, this);

	r_error = OK;
}

DisplayServerSwitch::~DisplayServerSwitch() {
	if (joypad) {
		memdelete(joypad);
		joypad = nullptr;
	}
	if (hook_cookie) {
		appletUnhook(hook_cookie);
		memdelete(hook_cookie);
		hook_cookie = nullptr;
	}
	if (gl_initialized) {
		gl_manager.finalize();
	}
}
