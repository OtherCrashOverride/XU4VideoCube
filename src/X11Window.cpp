#include "X11Window.h"

#include "Egl.h"
#include "Exception.h"
#include "OpenGL.h"
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <memory>
#include <GLES2/gl2.h>

X11Window::X11Window(int width, int height, const char* title)
{
    if (width < 1 || height < 1)
        throw Exception("Invalid window size.");


    this->width = width;
    this->height = height;


    // Get the X11 Display
    x11Display = XOpenDisplay(nullptr);

    if (x11Display == nullptr)
        throw Exception("XOpenDisplay failed.");


    // Get the EGLDisplay associate with the X11 Display
    eglDisplay = eglGetDisplay(x11Display);
    Egl::CheckError();


    // Intialize EGL for use on the display
    Egl::Initialize(eglDisplay);


    // Find a config that matches what we want
    EGLConfig config = Egl::PickConfig(eglDisplay);


    // Get the native visual id associated with the config
    EGLint xVisualId;

    eglGetConfigAttrib(eglDisplay, config, EGL_NATIVE_VISUAL_ID, &xVisualId);
    Egl::CheckError();

    XVisualInfo visTemplate = {0};
    visTemplate.visualid = xVisualId;

    int num_visuals;
    XVisualInfo* visInfoArray = XGetVisualInfo(x11Display,
                                VisualIDMask,
                                &visTemplate,
                                &num_visuals);

    if (num_visuals < 1 || visInfoArray == nullptr)
    {
        throw Exception("XGetVisualInfo failed.");
    }


    // Get the root window
    Window root = XRootWindow(x11Display, XDefaultScreen(x11Display));


    // Colormap
    XVisualInfo visInfo = visInfoArray[0];

    colormap = XCreateColormap(x11Display,
                               root,
                               visInfo.visual,
                               AllocNone);


    // Create the window
    XSetWindowAttributes attr = {0};
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = colormap;
    attr.event_mask = (ExposureMask | KeyReleaseMask | StructureNotifyMask);

    unsigned long mask = (CWBackPixel | CWBorderPixel | CWColormap | CWEventMask);

    xwin = XCreateWindow(x11Display,
                         root,
                         0,
                         0,
                         width,
                         height,
                         0,
                         visInfo.depth,
                         InputOutput,
                         visInfo.visual,
                         mask,
                         &attr);

    if (xwin == 0)
        throw Exception("XCreateWindow failed.");


    XWMHints hints = {0};
    hints.input = true;
    hints.flags = InputHint;

    XSetWMHints(x11Display, xwin, &hints);


    XStoreName(x11Display, xwin, title);
    XMapWindow(x11Display, xwin);

    wm_delete_window = XInternAtom(x11Display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x11Display, xwin, &wm_delete_window, 1);


    // Create the rendering surface
    EGLint windowAttr[] =
    {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };

    surface = eglCreateWindowSurface(eglDisplay, config, (EGLNativeWindowType)xwin, windowAttr);
    Egl::CheckError();

    if (surface == EGL_NO_SURFACE)
        throw Exception("eglCreateWindowSurface failed.");


    // Create a GLES2 context
    EGLint contextAttribs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs );
    if ( context == EGL_NO_CONTEXT )
    {
        throw Exception("eglCreateContext failed.");
    }


    // Make the context current
    if (!eglMakeCurrent(eglDisplay, surface, surface, context) )
    {
        throw Exception("eglMakeCurrent failed.");
    }
}

X11Window::~X11Window()
{
    // Release the context
    if (!eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) )
    {
        throw Exception("eglMakeCurrent failed.");
    }

    // Destory the context
    if (!eglDestroyContext(eglDisplay, context))
    {
        throw Exception ("eglDestroyContext failed.");
    }

    // Destroy the surface
    if (!eglDestroySurface(eglDisplay, surface))
    {
        throw Exception ("eglDestroySurface failed.");
    }

    // Destroy the window
    XDestroyWindow(x11Display, xwin);

    // Free colormap
    XFreeColormap(x11Display, colormap);

    // Disconnect
    XCloseDisplay(x11Display);
}


void X11Window::SwapBuffers()
{
    eglSwapBuffers(eglDisplay, surface);
    Egl::CheckError();
}

Action X11Window::ProcessMessages()
{
    Action result = Action::Nothing;

    if (XPending(x11Display) > 0)
    {
        XEvent xEvent;
        XNextEvent(x11Display, &xEvent);

        switch (xEvent.type)
        {
        case KeyRelease:
        {
            XKeyEvent keyEvent = xEvent.xkey;
            KeySym keySym = XkbKeycodeToKeysym(x11Display, keyEvent.keycode, 0, 0);

            switch (keySym)
            {
            case XK_space:
                result = Action::ChangeScene;
                break;

            case XK_Escape:
                result = Action::Quit;
                break;
            }
        }
        break;

        case ConfigureNotify:
        {
            XConfigureEvent configureEvent = xEvent.xconfigure;

            if (width != configureEvent.width ||
                    height != configureEvent.height)
            {
                // Resize
                width = configureEvent.width;
                height = configureEvent.height;

                glViewport(0, 0, width, height);
                OpenGL::CheckError();
            }
        }
        break;

        case ClientMessage:
            if ((Atom)xEvent.xclient.data.l[0] == wm_delete_window)
            {
                result = Action::Quit;
            }
            break;

        default:
            break;
        }
    }

    return result;
}
