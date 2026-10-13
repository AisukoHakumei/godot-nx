/**************************************************************************/
/*  gl_manager_switch.cpp                                                 */
/**************************************************************************/

#include "gl_manager_switch.h"

#include "switch_helpers.h"
#include "switch_wrapper.h"

#include "core/error/error_macros.h"
#include "core/os/os.h"

#include <cstdlib>

Error GLManagerSwitch::_create_surface(bool p_docked) {
	window_size = SwitchHelpers::window_size_for_docked(p_docked);
	NWindow *win = nwindowGetDefault();
	ERR_FAIL_COND_V_MSG(R_FAILED(nwindowSetDimensions(win, window_size.x, window_size.y)), ERR_CANT_CREATE, "nwindowSetDimensions failed.");

	surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)win, nullptr);
	ERR_FAIL_COND_V_MSG(surface == EGL_NO_SURFACE, ERR_CANT_CREATE, vformat("eglCreateWindowSurface failed: 0x%x", eglGetError()));

	ERR_FAIL_COND_V_MSG(!eglMakeCurrent(display, surface, surface, context), ERR_CANT_CREATE, vformat("eglMakeCurrent failed: 0x%x", eglGetError()));
	eglSwapInterval(display, use_vsync ? 1 : 0);
	return OK;
}

void GLManagerSwitch::_destroy_surface() {
	if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
		return;
	}
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(display, surface);
	surface = EGL_NO_SURFACE;
}

Error GLManagerSwitch::initialize(bool p_docked) {
#ifndef DEBUG_ENABLED
	setenv("MESA_NO_ERROR", "1", 1);
#endif
	if (OS::get_singleton()->is_stdout_verbose()) {
		setenv("EGL_LOG_LEVEL", "debug", 1);
	}

	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	ERR_FAIL_COND_V_MSG(display == EGL_NO_DISPLAY, ERR_CANT_CREATE, "eglGetDisplay returned no display.");

	EGLint major = 0, minor = 0;
	ERR_FAIL_COND_V_MSG(!eglInitialize(display, &major, &minor), ERR_CANT_CREATE, vformat("eglInitialize failed: 0x%x", eglGetError()));
	print_verbose(vformat("EGL %d.%d initialized.", major, minor));

	ERR_FAIL_COND_V_MSG(!eglBindAPI(EGL_OPENGL_ES_API), ERR_CANT_CREATE, "eglBindAPI(EGL_OPENGL_ES_API) failed.");

	const EGLint config_attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint num_configs = 0;
	if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, vformat("eglChooseConfig found no ES3 RGBA8/D24/S8 config: 0x%x", eglGetError()));
	}

	const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
	ERR_FAIL_COND_V_MSG(context == EGL_NO_CONTEXT, ERR_CANT_CREATE, vformat("eglCreateContext failed: 0x%x", eglGetError()));

	return _create_surface(p_docked);
}

void GLManagerSwitch::finalize() {
	if (display == EGL_NO_DISPLAY) {
		return;
	}
	_destroy_surface();
	if (context != EGL_NO_CONTEXT) {
		eglDestroyContext(display, context);
		context = EGL_NO_CONTEXT;
	}
	eglTerminate(display);
	display = EGL_NO_DISPLAY;
}

Error GLManagerSwitch::resize(bool p_docked) {
	_destroy_surface();
	Error err = _create_surface(p_docked);
	if (err != OK) {
		WARN_PRINT("EGL surface recreation failed, retrying once.");
		_destroy_surface();
		err = _create_surface(p_docked);
	}
	CRASH_COND_MSG(err != OK, "Could not recreate the EGL surface after a display change.");
	return err;
}

void GLManagerSwitch::make_current() {
	if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
		eglMakeCurrent(display, surface, surface, context);
	}
}

void GLManagerSwitch::release_current() {
	if (display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
}

void GLManagerSwitch::swap_buffers() {
	if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
		eglSwapBuffers(display, surface);
	}
}

void GLManagerSwitch::set_use_vsync(bool p_use) {
	use_vsync = p_use;
	if (display != EGL_NO_DISPLAY) {
		eglSwapInterval(display, p_use ? 1 : 0);
	}
}

GLManagerSwitch::~GLManagerSwitch() {
	finalize();
}
