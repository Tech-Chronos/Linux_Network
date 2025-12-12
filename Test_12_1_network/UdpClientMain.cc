#include "Log.hpp"

#include <iostream>
#include <string>

#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Log lg;

int main(int argc, char *argv[])
{
    // ./UdpClientMain IP PORT
    if (argc != 3)
    {
        std::cerr << "use method: [" << argv[0] << " IP PORT ]" << std::endl;
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
        // 客户端不需要绑定自身的 IP 和 Port，由OS自动分配
        while (true)
        {
            std::string message;
            getline(std::cin, message);
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));

            addr.sin_family = AF_INET;
            addr.sin_port = htons(std::stoi(argv[2]));
            addr.sin_addr.s_addr = inet_addr(argv[1]);
            int ret = sendto(sockfd, message.c_str(), message.size(), 0, (struct sockaddr *)&addr, sizeof addr);
            if (ret < 0)
            {
                LOG(FATAL, "sendto error!");
                exit(-1);
            }
            else
            {
                LOG(DEBUG, "send successfully!");
                char buffer[1024];
                struct sockaddr_in tmp;
                memset(&tmp, 0, sizeof tmp);
                socklen_t tmp_len;
                int n = recvfrom(sockfd, buffer, sizeof buffer, 0, (struct sockaddr *)&tmp, &tmp_len);
                if (n < 0)
                {
                    LOG(FATAL, "recvfrom error!");
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