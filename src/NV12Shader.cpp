#include "NV12Shader.h"

#include "OpenGL.h"
#include "Exception.h"


const char* NV12Shader::vertexSource = "\n \
attribute mediump vec4 Attr_Position;\n \
attribute mediump vec2 Attr_TexCoord0;\n \
\n \
uniform mat4 WorldViewProjection;\n \
uniform mediump vec2 TextureScale;\n \
uniform mediump vec2 TextureOffset;\n \
\n \
varying mediump vec2 TexCoord0;\n \
\n \
void main()\n \
{\n \
\n \
  gl_Position = Attr_Position *  WorldViewProjection;\n \
  TexCoord0 = Attr_TexCoord0 * TextureScale + TextureOffset;\n \
}\n \
\n \
 ";

#if 0
const char* NV12Shader::fragmentSource = "\n \
uniform lowp sampler2D DiffuseMap;\n \
uniform lowp sampler2D VUMap;\n \
\n \
varying mediump vec2 TexCoord0;\n \
\n \
void main()\n \
{\n \
  mediump float y = texture2D(DiffuseMap, TexCoord0).r;\n \
  mediump float u = texture2D(VUMap, TexCoord0).r;\n \
  mediump float v = texture2D(VUMap, TexCoord0).a;\n \
\n \
  // http://stackoverflow.com/questions/17105386/using-opengl-es-shader-to-convert-yuv-to-rgb\n \
\n \
  y = 1.1643 * (y - 0.0625);\n \
  u -= 0.5;\n \
  v -= 0.5;\n \
\n \
  lowp float r = y + 1.5958 * v;\n \
  lowp float g = y - 0.39173 * u - 0.81290 * v;\n \
  lowp float b = y + 2.017 * u;\n \
\n \
  lowp vec4 rgb = vec4(r, g, b, 1.0);\n \
  gl_FragColor = rgb;\n \
}\n \
\n \
";
#else
const char* NV12Shader::fragmentSource = "#extension GL_OES_EGL_image_external : require\n \
uniform lowp samplerExternalOES DiffuseMap;\n \
\n \
varying mediump vec2 TexCoord0;\n \
\n \
void main()\n \
{\n \
  lowp vec4 rgb = texture2D(DiffuseMap, TexCoord0);\n \
  gl_FragColor = rgb;\n \
}\n \
\n \
";
#endif

GLuint NV12Shader::CreateShader(GLenum shaderType, const char* sourceCode)
{
    GLuint openGLShaderID = glCreateShader(shaderType);
    OpenGL::CheckError();

    const char* glSrcCode[] = { sourceCode };
    const int lengths[] { -1 }; // Tell OpenGL the string is NULL terminated

    glShaderSource(openGLShaderID, 1, glSrcCode, lengths);
    OpenGL::CheckError();

    glCompileShader(openGLShaderID);
    OpenGL::CheckError();


    GLint param;

    glGetShaderiv(openGLShaderID, GL_COMPILE_STATUS, &param);
    OpenGL::CheckError();

    if (param == GL_FALSE)
    {
        throw Exception("Shader Compilation Failed.");
    }

    return openGLShaderID;
}

GLuint NV12Shader::CreateProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint openGLProgramID = glCreateProgram();
    OpenGL::CheckError();

    glAttachShader(openGLProgramID, vertexShader);
    OpenGL::CheckError();

    glAttachShader(openGLProgramID, fragmentShader);
    OpenGL::CheckError();

    return openGLProgramID;
}

GLuint NV12Shader::Create()
{
    GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, fragmentSource);

    return CreateProgram(vertexShader, fragmentShader);
}
