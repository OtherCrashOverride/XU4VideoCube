#include "OpenGLException.h"

#include <GLES2/gl2.h>

const char* OpenGLException::GetErrorName(int errorCode)
{
    const char* result;

    switch (errorCode)
    {
    case GL_INVALID_ENUM:
        result = "GL_INVALID_ENUM";
        break;

    case GL_INVALID_VALUE:
        result = "GL_INVALID_VALUE";
        break;

    case GL_INVALID_OPERATION:
        result = "GL_INVALID_OPERATION";
        break;

    case GL_OUT_OF_MEMORY:
        result = "GL_OUT_OF_MEMORY";
        break;

    case GL_INVALID_FRAMEBUFFER_OPERATION: //               0x0506
        result = "GL_INVALID_FRAMEBUFFER_OPERATION";
        break;

    default:
        result = "(Unknown)";
        break;
    }

    return result;
}

OpenGLException::OpenGLException(int errorCode)
    : Exception(GetErrorName(errorCode)),
      errorCode(errorCode)
{


}

