#include <iostream>
#include <unistd.h>
#include <sys/types.h>

#include "Common.hpp"

int main()
{
    NonBlock(0);
    while (true)
    {
        std::cout << "Please Enter: ";

        fflush(stdout);
        char buffer[1024];
        int ret = read(0, buffer, sizeof(buffer) - 1);
        if (ret > 0)
        {
            buffer[ret] = 0;
            std::cout << "echo: " << buffer;
            sleep(1);
        }
        else if (ret == 0)
        {   
            std::cout << "read quit!" << std::endl;
            break;
        }
        else
        {
            if (errno == EWOULDBLOCK)
            {
                std::cout << "repeat wait!" << std::endl;
                sleep(1);
                continue;
            }
            std::cout << "read error!" << std::endl;
        }
    }
    return 0;
}