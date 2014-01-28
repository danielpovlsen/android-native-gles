#include "shader_utils.h"
#include "log.h"

#include <stdlib.h>

GLuint compileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	if (!shader) {
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compileStatus;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

	if (compileStatus == GL_TRUE) {
		return shader;
	}

	GLint infoLogLength = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength) {
		char* infoLog = malloc(infoLogLength);
		if (infoLog) {
			glGetShaderInfoLog(shader, infoLogLength, NULL, infoLog);
			LOGE("Could not compile shader %d:\n%s", type, infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
	}
	return 0;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
	if (!vertexShader) {
		return 0;
	}

	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (!fragmentShader) {
		return 0;
	}

	GLuint program = glCreateProgram();
	if (!program) {
		return 0;
	}

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	GLint linkStatus;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus == GL_TRUE) {
		return program;
	}

	GLint infoLogLength = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength) {
		char* infoLog = malloc(infoLogLength);
		if (infoLog) {
			glGetProgramInfoLog(program, infoLogLength, NULL, infoLog);
			LOGE("Could not link program:\n%s", infoLog);
			free(infoLog);
		}
	}
	glDeleteProgram(program);
	return 0;
}
