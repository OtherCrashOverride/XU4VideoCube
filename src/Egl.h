#pragma once

#include <X11/X.h>
#include <EGL/egl.h>

class Egl
{
public:
    static void CheckError(bool apiResult);
    static void CheckError();
    static EGLConfig PickConfig(EGLDisplay eglDisplay);
    static void Initialize(EGLDisplay eglDisplay);
};
