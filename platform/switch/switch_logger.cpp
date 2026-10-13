/**************************************************************************/
/*  switch_logger.cpp                                                     */
/**************************************************************************/

#include "switch_logger.h"

#include "switch_wrapper.h"

#include <cstdio>
#include <cstring>

void SwitchLogger::logv(const char *p_format, va_list p_list, bool p_err) {
	if (!should_log(p_err)) {
		return;
	}

	const int static_buf_size = 1024;
	char static_buf[static_buf_size];
	char *buf = static_buf;
	va_list list_copy;
	va_copy(list_copy, p_list);
	int len = vsnprintf(buf, static_buf_size, p_format, p_list);
	if (len >= static_buf_size) {
		buf = (char *)Memory::alloc_static(len + 1);
		vsnprintf(buf, len + 1, p_format, list_copy);
	}
	va_end(list_copy);

	fputs(buf, p_err ? stderr : stdout);
	svcOutputDebugString(buf, strlen(buf));

	if (len >= static_buf_size) {
		Memory::free_static(buf);
	}
}
