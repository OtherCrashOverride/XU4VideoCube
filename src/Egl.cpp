#include "Egl.h"

#include "EglException.h"
#include <memory>



void Egl::CheckError(bool apiResult)
{
    if (apiResult == false)
    {
        EGLenum error = eglGetError();
        throw EglException(error);
    }
}

void Egl::CheckError()
{
    EGLenum error = eglGetError();

    if (error != EGL_SUCCESS)
    {
        throw EglException(error);
    }
}

EGLConfig Egl::PickConfig(EGLDisplay eglDisplay)
{
    if (eglDisplay == nullptr)
        throw Exception("eglDisplay is null.");


    EGLint configAttribs[] =
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    0,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_NONE
    };


    // Determine config selection count
    EGLint configCount;
    Egl::CheckError(eglChooseConfig(eglDisplay,
                                    configAttribs,
                                    nullptr,
                                    0,
                                    &configCount));

    if (configCount < 1)
        throw Exception("No matching configs.");


    // Get the total configs
    EGLConfig configs[configCount];
    Egl::CheckError(eglChooseConfig(eglDisplay,
                                    configAttribs,
                                    configs,
                                    configCount,
                                    &configCount));


    // Find a matching config
    EGLConfig config = nullptr;
    bool isFound = false;

    for (int i = 0; i < configCount; ++i)
    {
        config = configs[i];

        // Test color format
        EGLint colorSize;

        eglGetConfigAttrib(eglDisplay, config, EGL_RED_SIZE, &colorSize);
        if (colorSize != 8)
        {
            continue;
        }
        eglGetConfigAttrib(eglDisplay, config, EGL_GREEN_SIZE, &colorSize);
        if (colorSize != 8)
        {
            continue;
        }

        eglGetConfigAttrib(eglDisplay, config, EGL_BLUE_SIZE, &colorSize);
        if (colorSize != 8)
        {
            continue;
        }

        eglGetConfigAttrib(eglDisplay, config, EGL_ALPHA_SIZE, &colorSize);
        if (colorSize != 8)
        {
            continue;
        }


        // Test depth
        EGLint configDepthSize;
        eglGetConfigAttrib(eglDisplay, config, EGL_DEPTH_SIZE, &configDepthSize);
        if (configDepthSize != 24)
        {
            continue;
        }


        // Test stencil
        EGLint configStencilSize;
        eglGetConfigAttrib(eglDisplay, config, EGL_STENCIL_SIZE, &configStencilSize);
        if (configStencilSize != 0)
        {
            continue;
        }

        // Test samples
        EGLint sampleSize;
        eglGetConfigAttrib(eglDisplay, config, EGL_SAMPLES, &sampleSize);
        if (sampleSize > 1)
        {
            continue;
        }

        // A match was found
        isFound = true;
        break;
    }


    // if no matching config was found, throw
    if (!isFound)
        throw Exception("No matching EGLConfig found.");

    return config;
}

void Egl::Initialize(EGLDisplay eglDisplay)
{
    // initialize the EGL display connection
    EGLint major;
    EGLint minor;

    EGLBoolean api = eglInitialize(eglDisplay, &major, &minor);
    Egl::CheckError(api);


    //printf("EGL: major=%d, minor=%d\n", major, minor);
    //printf("EGL: Vendor=%s\n", eglQueryString(eglDisplay, EGL_VENDOR));
    //printf("EGL: Version=%s\n", eglQueryString(eglDisplay, EGL_VERSION));
    //printf("EGL: ClientAPIs=%s\n", eglQueryString(eglDisplay, EGL_CLIENT_APIS));
    //printf("EGL: Extensions=%s\n", eglQueryString(eglDisplay, EGL_EXTENSIONS));
    //printf("\n");
}
