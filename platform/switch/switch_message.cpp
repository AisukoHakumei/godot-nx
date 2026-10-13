/**************************************************************************/
/*  switch_message.cpp                                                    */
/**************************************************************************/

#include "switch_message.h"

#include "switch_wrapper.h"

#include <cstdio>

const char *const SWITCH_APPLET_MODE_MESSAGE = "Launch this game via title override (hold R while opening a title).";

bool switch_is_application_mode() {
	AppletType type = appletGetAppletType();
	return type == AppletType_Application || type == AppletType_SystemApplication;
}

void switch_show_blocking_message(const String &p_text) {
	consoleInit(nullptr);

	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	PadState pad;
	padInitializeDefault(&pad);

	printf("\n\n  %s\n\n  Press + to exit.\n", p_text.utf8().get_data());
	consoleUpdate(nullptr);

	while (appletMainLoop()) {
		padUpdate(&pad);
		if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
			break;
		}
		consoleUpdate(nullptr);
	}

	consoleExit(nullptr);
}
