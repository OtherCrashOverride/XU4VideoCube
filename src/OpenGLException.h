#pragma once

#include "Exception.h"

class OpenGLException : Exception
{
    static const char* GetErrorName(int errorCode);

    const int errorCode;


public:

    int GetErrorCode() const
    {
        return errorCode;
    }

    OpenGLException(int errorCode);

};
