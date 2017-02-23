#pragma once

//#include <GLES2/gl2.h>
#include "OpenGL.h"

class NV12Shader
{
        static const char* vertexSource;

        static const char* fragmentSource;

        static GLuint CreateShader(GLenum shaderType, const char* sourceCode);

        static GLuint CreateProgram(GLuint vertexShader, GLuint fragmentShader);

public:
    static GLuint Create();


};
