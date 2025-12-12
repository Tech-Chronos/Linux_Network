#include "Log.hpp"
#include "InAddr.hpp"
#include <iostream>
#include <string>
#include <memory>

#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        LOG(FATAL, "USAGE -> %s IP PORT", argv[0]);
        exit(-1);
    }
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        LOG(FATAL, "socket error!");
        exit(-1);
    }
    else
    {
        while (true)
        {
            std::string message;
            std::cout << "Please input message: ";
            getline(std::cin, message);

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(std::stoi(argv[2]));
            addr.sin_addr.s_addr = inet_addr(argv[1]);

            int ret = sendto(sockfd, message.c_str(), message.size(), 0, (sockaddr *)&addr, sizeof addr);
            if (ret < 0)
            {
                LOG(FATAL, "sendto error!");
                exit(-1);
            }
            else
            {
                sockaddr_in tmp;
                socklen_t len = sizeof(tmp);
                LOG(DEBUG, "send success!");
                char buffer[1024];
                int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr *)&tmp, &len);
                if (n < 0)
                {
                    LOG(FATAL, "recvfrom error!");
                    exit(-1);
                }
                else
                {
                    buffer[n] = '\0';
                    LOG(DEBUG, "%s", buffer);
                }
            }
        }
    }
    return 0;
}