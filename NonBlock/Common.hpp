#pragma once
#include <unistd.h>
#include <fcntl.h>

void NonBlock(int fd)
{   
    int flags = fcntl(fd, F_GETFL);

    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}