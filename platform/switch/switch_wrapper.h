/**************************************************************************/
/*  switch_wrapper.h                                                      */
/**************************************************************************/

#pragma once

// libnx defines C types whose names collide with Godot classes. Rename them
// for the duration of the libnx include; libnx is C, so the struct tags do
// not affect its ABI. Always include this header instead of <switch.h>.

#define Thread NxThread
#define Mutex NxMutex
#define RMutex NxRMutex
#define Semaphore NxSemaphore
#define CondVar NxCondVar
#define Event NxEvent

#include <switch.h>

#undef Thread
#undef Mutex
#undef RMutex
#undef Semaphore
#undef CondVar
#undef Event
#undef BIT
