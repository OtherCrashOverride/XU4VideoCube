#pragma once

#include "Exception.h"
#include <string>


class EglException : Exception
{
public:
    static std::string ConvertEglErrorNumber(int error);



    int error;

    int GetError() const
    {
        return error;
    }


    EglException(int error)
        : Exception(ConvertEglErrorNumber(error)),
          error(error)
    {
    }
};



