#pragma once

#include <iostream>
#include <unistd.h>
#include <fcntl.h>

void SetNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL);

    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}