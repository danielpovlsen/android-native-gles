#include "log.h"
#include "android_native_app_glue.h"
#include "shader_utils.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

const char vertexShader[] = 
	"attribute vec4 position;\n"
	"varying vec3 color;\n"
	"void main() {\n"
	"	color = position.xyz*0.5 + vec3(0.5);\n"
	"	gl_Position = position;\n"
	"}\n";

const char fragmentShader[] = 
	"precision mediump float;\n"
	"varying vec3 color;\n"
	"void main() {\n"
	"	gl_FragColor = vec4(color, 1.0);\n"
	"}\n";

const GLfloat triangleVertices[] = {
	 0.0f,  0.5f,
	-0.5f, -0.5f,
	 0.5f, -0.5f
};

struct SavedState {
	float x;
	float y;
};

struct GLObjects {
	GLuint program;
	GLuint positionLocation;
};

struct AppState {
	android_app* app;
	bool windowInitialized;
	bool resumed;
	bool focused;
	bool running;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	int32_t width;
	int32_t height;
	SavedState savedState;
	GLObjects glObjects;
};

void printGLString(const char* name, GLenum e) {
	const GLubyte* s = glGetString(e);
	LOGI("GL %s = %s", name, s);
}

bool initDisplay(AppState* appState) {
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(display, 0, 0);

	const EGLint configAttribs[] = {
		//		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		//		EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
		//		EGL_BLUE_SIZE, 8,
		//		EGL_GREEN_SIZE, 8,
		//		EGL_RED_SIZE, 8,
		EGL_NONE
	};
	EGLConfig config;
	EGLint numConfigs;
	eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);

	EGLint format;
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

	ANativeWindow_setBuffersGeometry(appState->app->window, 0, 0, format);

	EGLSurface surface = eglCreateWindowSurface(display, config, appState->app->window, NULL);

	EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
	if (context == EGL_NO_CONTEXT) {
		LOGE("eglCreateContext failed with error 0x%04x", eglGetError());
		return false;
	}

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		LOGE("eglMakeCurrent failed with error 0x%04x", eglGetError());
		return false;
	}

	appState->display = display;
	appState->context = context;
	appState->surface = surface;

	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);

	GLuint program = createProgram(vertexShader, fragmentShader);
	if (!program) {
		LOGE("Could not create program");
		return false;
	}

	appState->glObjects.program = program;
	appState->glObjects.positionLocation = glGetAttribLocation(program, "position");

	return true;
}

void updateViewportIfNecessary(AppState* appState) {
	// seemingly a bug with Android (10?) that sometimes the resize / config change event is late / missing
	// fortunately safe, cheap to check every frame
	int32_t w = ANativeWindow_getWidth(appState->app->window);
	int32_t h = ANativeWindow_getHeight(appState->app->window);
	if (appState->width != w || appState->height != h) {
		appState->width = w;
		appState->height = h;
		glViewport(0, 0, w, h);
	}
}

void drawFrame(AppState* appState) {
	updateViewportIfNecessary(appState);

	float x = appState->savedState.x;
	float y = appState->savedState.y;
	if (x > 1.0f && y > 1.0f) { 
		x /= appState->width;
		y /= appState->height;
	} else {
		// assume stick in range [-1:1]
		x = x * 0.5f + 0.5f;
		y = y * 0.5f + 0.5f;
	}

	glClearColor(x, 1.0f - y, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(appState->glObjects.program);
	glVertexAttribPointer(appState->glObjects.positionLocation, 2, GL_FLOAT, GL_FALSE, 0, triangleVertices);
	glEnableVertexAttribArray(appState->glObjects.positionLocation);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	bool drawMovingBlock = true;
	bool drawPointer = true;

	if (drawMovingBlock) {
		static int blockX = 0;
		glEnable(GL_SCISSOR_TEST);
		glScissor(blockX, 0, 8, 8);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
		blockX = (blockX + 1) % appState->width;
	}

	if (drawPointer) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(x*appState->width, (1.0f - y)*appState->height, 8, 8);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glScissor(x*appState->width + 2, (1.0f - y)*appState->height + 2, 4, 4);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	eglSwapBuffers(appState->display, appState->surface);
}

void termDisplay(AppState* appState) {
	if (appState->display != EGL_NO_DISPLAY) {
		eglMakeCurrent(appState->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (appState->context != EGL_NO_CONTEXT) {
			eglDestroyContext(appState->display, appState->context);
		}
		if (appState->surface != EGL_NO_SURFACE) {
			eglDestroySurface(appState->display, appState->surface);
		}
		eglTerminate(appState->display);
	}
	appState->display = EGL_NO_DISPLAY;
	appState->context = EGL_NO_CONTEXT;
	appState->surface = EGL_NO_SURFACE;
}

int32_t onInputEvent(android_app* app, AInputEvent* event) {
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		size_t pointerCount = AMotionEvent_getPointerCount(event);

		for (size_t i = 0; i < pointerCount; ++i) {
			int32_t pointerId = AMotionEvent_getPointerId(event, i);
			float x = AMotionEvent_getX(event, i);
			float y = AMotionEvent_getY(event, i);

			LOGI("Received motion event from pointer %d: (%.2f, %.2f)", pointerId, x, y);

			AppState* appState = static_cast<AppState*>(app->userData);
			appState->savedState.x = x;
			appState->savedState.y = y;
		}
		return 1;
	} else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
		LOGI("Received key event: %d", AKeyEvent_getKeyCode(event));
		return 1;
	}
	return 0;
}

static void onAppCmd(android_app* app, int32_t cmd) {
	AppState* appState = static_cast<AppState*>(app->userData);

	switch (cmd) {
	case APP_CMD_START:
		LOGI("APP_CMD_START");
		break;
	case APP_CMD_RESUME:
		LOGI("APP_CMD_RESUME");
		appState->resumed = true;
		break;
	case APP_CMD_PAUSE:
		LOGI("APP_CMD_PAUSE");
		appState->resumed = false;
		break;
	case APP_CMD_STOP:
		LOGI("APP_CMD_STOP");
		break;
	case APP_CMD_DESTROY:
		LOGI("APP_CMD_DESTROY");
		break;
	case APP_CMD_GAINED_FOCUS:
		LOGI("APP_CMD_GAINED_FOCUS");
		appState->focused = true;
		break;
	case APP_CMD_LOST_FOCUS:
		LOGI("APP_CMD_LOST_FOCUS");
		appState->focused = false;
		break;
	case APP_CMD_INIT_WINDOW:
		LOGI("APP_CMD_INIT_WINDOW");
		if (appState->app->window != NULL) {
			initDisplay(appState);
		}
		appState->windowInitialized = true;
		break;
	case APP_CMD_WINDOW_RESIZED:
		LOGI("APP_CMD_WINDOW_RESIZED");
		break;
	case APP_CMD_TERM_WINDOW:
		LOGI("APP_CMD_TERM_WINDOW");
		appState->windowInitialized = false;
		termDisplay(appState);
		break;

	case APP_CMD_SAVE_STATE:
		LOGI("APP_CMD_SAVE_STATE");
		appState->app->savedState = malloc(sizeof(SavedState));
		appState->app->savedStateSize = sizeof(SavedState);
		*static_cast<SavedState*>(appState->app->savedState) = appState->savedState;
		break;
	case APP_CMD_CONFIG_CHANGED:
		LOGI("APP_CMD_CONFIG_CHANGED");
		break;
	default:
		LOGI("Unknown CMD: %d", cmd);
	}
	appState->running = (appState->resumed && appState->windowInitialized && appState->focused);
}

void android_main(android_app* app) {
	LOGI("--- MAIN THREAD STARTED ---");

	app_dummy(); // Ensure glue code isn't stripped

	AppState appState;
	memset(&appState, 0, sizeof(appState));
	app->userData = &appState;
	app->onInputEvent = onInputEvent;
	app->onAppCmd = onAppCmd;
	appState.app = app;

	if (app->savedState != NULL) {
		// Restore a previously saved state
		appState.savedState = *static_cast<SavedState*>(app->savedState);
	}

	while (true) {
		int ident;
		int fd;
		int events;
		android_poll_source* source;

		while((ident = ALooper_pollAll(appState.running ? 0 : -1, &fd, &events, reinterpret_cast<void**>(&source))) >= 0) {
			// process this event
			if (source) {
				source->process(app, source);
			}

			if (app->destroyRequested != 0) {
				termDisplay(&appState);
				return;
			}
		}

		if (appState.running) {
			drawFrame(&appState);
		}
	}
}
