#include "OpenGL.h"

#include <GLES2/gl2.h>
#include "OpenGLException.h"

void OpenGL::CheckError()
{
#ifdef DEBUG
    int error = glGetError();

    if (error != GL_NO_ERROR)
    {
        throw OpenGLException(error);
    }
#endif
}
