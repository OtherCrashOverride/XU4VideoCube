#include "EglException.h"

#include <EGL/egl.h>


std::string EglException::ConvertEglErrorNumber(int error)
{
    std::string msg;

    switch (error)
    {
    case EGL_SUCCESS:
        msg = "EGL_SUCCESS";
        break;

    case EGL_NOT_INITIALIZED:
        msg = "EGL_NOT_INITIALIZED";
        break;

    case EGL_BAD_ACCESS:
        msg = "EGL_BAD_ACCESS";
        break;

    case EGL_BAD_ALLOC:
        msg = "EGL_BAD_ALLOC";
        break;

    case EGL_BAD_ATTRIBUTE:
        msg = "EGL_BAD_ATTRIBUTE";
        break;

    case EGL_BAD_CONFIG:
        msg = "EGL_BAD_CONFIG";
        break;

    case EGL_BAD_CONTEXT:
        msg = "EGL_BAD_CONTEXT";
        break;

    case EGL_BAD_CURRENT_SURFACE:
        msg = "EGL_BAD_CURRENT_SURFACE";
        break;

    case EGL_BAD_DISPLAY:
        msg = "EGL_BAD_DISPLAY";
        break;

    case EGL_BAD_MATCH:
        msg = "EGL_BAD_MATCH";
        break;

    case EGL_BAD_NATIVE_PIXMAP:
        msg = "EGL_BAD_NATIVE_PIXMAP";
        break;

    case EGL_BAD_NATIVE_WINDOW:
        msg = "EGL_BAD_NATIVE_WINDOW";
        break;

    case EGL_BAD_PARAMETER:
        msg = "EGL_BAD_PARAMETER";
        break;

    case EGL_BAD_SURFACE:
        msg = "EGL_BAD_SURFACE";
        break;

    case EGL_CONTEXT_LOST:
        msg = "EGL_CONTEXT_LOST";
        break;

    default:
        msg = "(unknown)";
        break;
    }

    return msg;
}

