/**************************************************************************/
/*  switch_message.h                                                      */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

// Full-screen text shown with libnx's console on the default NWindow.
// Only usable BEFORE GLManagerSwitch owns the window: the applet-mode guard
// and the missing-pck error both run before the renderer initialises.
// Blocks until the player presses Plus, then returns.
void switch_show_blocking_message(const String &p_text);

// True when the process runs with the full application memory pool
// (title override / installed title) rather than as an applet (album/hbmenu).
bool switch_is_application_mode();

extern const char *const SWITCH_APPLET_MODE_MESSAGE;
