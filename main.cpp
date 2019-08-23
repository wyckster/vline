#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <fstream>
#include <string>
#include <iostream>

static int x = 0;
static GLint xform_location;

void FATAL(const std::string& msg) {
  std::cerr << msg << std::endl;
  exit(-1);
}

void draw(int width) {
  // Render a vertical line
  glClearColor(0.0f, 0.0f, 0.5f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUniform4f(xform_location, 16/(float)width, 2.0f * 1.0f, -1.0 + 2.0 * (x/(float)width), -1.0 + 2.0 * 0.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Update position of line
  x += 16;
  if (x + 16 > width) x = 0;
}

int main(int argc, char **argv) {

  Display* display = XOpenDisplay(NULL);
  if (!display) FATAL("XOpenDisplay Failed");

  int screenNum = DefaultScreen(display);
  int displayWidth = DisplayWidth(display, screenNum);
  int displayHeight = DisplayHeight(display, screenNum);
  int displayBitDepth = DefaultDepth(display, screenNum);
  int displayXOffset = 0;
  int displayYOffset = 0;

  XSetWindowAttributes windowAttributes;
  windowAttributes.background_pixel = BlackPixel(display, screenNum);
	windowAttributes.override_redirect = 1;

  Window window =
      XCreateWindow(display, DefaultRootWindow(display), displayXOffset,
                    displayYOffset, displayWidth, displayHeight, 0,
                    displayBitDepth, CopyFromParent, CopyFromParent,
                    (CWBackPixel | CWOverrideRedirect), &windowAttributes);

  // Extra stuff
  {
    XSelectInput(display, (int32_t)window, ExposureMask);
    XMapWindow(display, (int32_t)window);
    GC gc = XCreateGC(display, window, 0, NULL);

    XSetForeground(display, gc, WhitePixel(display, screenNum));
    XFontStruct* fontInfo = XLoadQueryFont(display, "9x15bold");
  }

  // EGL stuff
  {
  	EGLDisplay eglDisplay = eglGetDisplay(display);
    if (eglDisplay == EGL_NO_DISPLAY) FATAL("eglGetDisplay failed");
    EGLBoolean result = eglInitialize(eglDisplay, 0, 0);
    if (!result) FATAL("eglInitialize failed");
    int numConfig = 0;
    EGLConfig eglConfig = NULL;
    static EGLint rgba8888[] = {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE,
    };
    result = eglChooseConfig(eglDisplay, rgba8888, &eglConfig, 1, &numConfig);
    if (!result) FATAL("eglChooseConfig failed");

  	EGLint eglContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, eglContextAttribs);
    int eglErr = eglGetError();
    if (eglErr != EGL_SUCCESS) FATAL("eglCreateContext failed");

    EGLSurface eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, (EGLNativeWindowType)window, NULL);
    eglErr = eglGetError();
    if (eglSurface == EGL_NO_SURFACE) FATAL("eglCreateWindowSurface failed");

    result = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    if (!result) FATAL("eglMakeCurrent failed");

    GLuint program = glCreateProgram();

    static const char kVertexShader[] = R"sh4d3r(

      attribute vec4 in_pos;
      precision mediump float;
      uniform vec4 xform;

      void main() {
        vec4 t = in_pos;
        t.xy *= xform.xy;
        t.xy += xform.zw;
        gl_Position = t;
      }

    )sh4d3r";

    static const char kFragmentShader[] = R"sh4d3r(

      #extension GL_OES_EGL_image_external : require
      precision mediump float;

      void main() {
        gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
      }

    )sh4d3r";

    auto createShader = [&](GLuint program, GLenum type, const char *source, size_t size) {
      char log[4096];
      int result = GL_FALSE;

      GLuint shader = glCreateShader(type);

      glShaderSource(shader, 1, &source, NULL);
      glCompileShader(shader);
      glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
      if (!result) {
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        FATAL("Error while compiling OpenGL Shader: " + std::string(log));
      }
      glAttachShader(program, shader);
      int glErr = glGetError();
      if (glErr != GL_NO_ERROR) {
        FATAL("Error while attaching OpenGL Shader: " + std::to_string(glErr));
      }
    };
    createShader(program, GL_VERTEX_SHADER, kVertexShader, sizeof(kVertexShader));
    createShader(program, GL_FRAGMENT_SHADER, kFragmentShader, sizeof(kFragmentShader));

    glLinkProgram(program);
    int glErr = glGetError();
    if (glErr != GL_NO_ERROR) FATAL("Unable to glLinkProgram(), error: " + std::to_string(glErr));

    GLint linkStatus = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (!linkStatus) {
      char log[4096];
      glGetShaderInfoLog(program, sizeof(log), NULL, log);
      FATAL("Error while Linking OpenGL Shader: " + std::string(log));
    }

    glUseProgram(program);
    glErr = glGetError();
    if (glErr != GL_NO_ERROR) FATAL("Unable to glUseProgram(), error: " + std::to_string(glErr));

    GLint pos_location = glGetAttribLocation(program, "in_pos");
    glEnableVertexAttribArray(pos_location);
    static const float kVertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };

    glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

    xform_location = glGetUniformLocation(program, "xform");
    glUniform4f(xform_location, 1.0f, 1.0f, 0.0f, 0.0f);

    glViewport(0, 0, displayWidth, displayHeight);

    while (true) {
      draw(displayWidth);
      eglSwapBuffers(eglDisplay, eglSurface);
    }
  }
}
