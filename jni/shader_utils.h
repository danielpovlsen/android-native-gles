#pragma once

#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

GLuint createProgram(const char* vertexSource, const char* fragmentSource);

#ifdef __cplusplus
}
#endif
