#include "Exception.h"

#include <stdio.h>

Exception::Exception()
: exception()
{
}

Exception::Exception(std::string message)
:exception()
{
    printf("%s\n", message.c_str());
}

Exception::Exception(const char* message)
{
    printf("%s\n", message);
}
