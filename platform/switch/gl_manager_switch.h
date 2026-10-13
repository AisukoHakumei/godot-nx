/**************************************************************************/
/*  gl_manager_switch.h                                                   */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/math/vector2i.h"

#include "platform_egl.h"

// Owns the EGL display, surface and ES 3.1 context on libnx's default
// NWindow. The window size comes from the applet operation mode because
// eglQuerySurface returns 0 on switch-mesa 20.1. This is the one unit to
// replace if the GPU path ever changes (spec section 3).
class GLManagerSwitch {
	EGLDisplay display = EGL_NO_DISPLAY;
	EGLConfig config = nullptr;
	EGLSurface surface = EGL_NO_SURFACE;
	EGLContext context = EGL_NO_CONTEXT;
	Size2i window_size;
	bool use_vsync = true;

	Error _create_surface(bool p_docked);
	void _destroy_surface();

public:
	Error initialize(bool p_docked);
	void finalize();

	Error resize(bool p_docked);

	void make_current();
	void release_current();
	void swap_buffers();

	void set_use_vsync(bool p_use);
	bool is_using_vsync() const { return use_vsync; }

	Size2i get_window_size() const { return window_size; }

	~GLManagerSwitch();
};
