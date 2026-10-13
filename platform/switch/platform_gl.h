/**************************************************************************/
/*  platform_gl.h                                                         */
/**************************************************************************/

#pragma once

#ifndef GLES_API_ENABLED
#define GLES_API_ENABLED // Allow using GLES.
#endif

// IWYU pragma: begin_exports.
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
// IWYU pragma: end_exports.
