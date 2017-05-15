#pragma once

#include <EGL/egl.h>

enum class Action
{
    Nothing = 0,    //None is a #define
    Quit,
    ChangeScene

};



class X11Window
{
    int width = 0;
    int height = 0;
    Display* x11Display = 0;
    Window xwin = 0;
    Colormap colormap = 0;
    EGLDisplay eglDisplay = 0;
    EGLSurface surface = 0;
    EGLContext context = 0;
    Atom wm_delete_window;

public:

	Display* X11Display() const
	{
		return x11Display;
	}

	EGLDisplay EglDisplay() const
	{
		return eglDisplay;
	}


    X11Window(int width, int height, const char* title);
    virtual ~X11Window();


    void SwapBuffers();
    Action ProcessMessages();
};

