#pragma once

#include <exception>
#include <string>

class Exception : std::exception
{

public:
    Exception();
    Exception(std::string message);
    Exception(const char* message);
};
