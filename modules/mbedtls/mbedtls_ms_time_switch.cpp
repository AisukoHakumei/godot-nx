/**************************************************************************/
/*  mbedtls_ms_time_switch.cpp                                            */
/**************************************************************************/

#ifdef __SWITCH__

// mbedtls' platform_util.c has no millisecond clock for Horizon; provide it
// (enabled by MBEDTLS_PLATFORM_MS_TIME_ALT in godot_module_mbedtls_config.h).

#include <mbedtls/platform_time.h>

#include <time.h>

extern "C" mbedtls_ms_time_t mbedtls_ms_time(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (mbedtls_ms_time_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif // __SWITCH__
