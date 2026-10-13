/**************************************************************************/
/*  switch_logger.h                                                       */
/**************************************************************************/

#pragma once

#include "core/io/logger.h"

// Mirrors every log line to stdout (nxlink when connected) and to the
// kernel debug log (svcOutputDebugString), which emulators display.
class SwitchLogger : public Logger {
public:
	virtual void logv(const char *p_format, va_list p_list, bool p_err) override _PRINTF_FORMAT_ATTRIBUTE_2_0;
};
