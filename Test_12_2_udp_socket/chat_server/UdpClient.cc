#include "Log.hpp"
#include "InAddr.hpp"
#include "Thread.hpp"
#include <iostream>
#include <string>
#include <memory>

#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using client_func = std::function<void(std::string& name)>;

void receive_func(int sockfd, std::string& thread_name)
{
    while (true)
    {
        char buffer[1024];
        bzero(buffer, sizeof buffer);
        sockaddr_in server;
        bzero(&server, sizeof server);

        socklen_t len = sizeof server;
        int n = recvfrom(sockfd, buffer, sizeof buffer, 0, (sockaddr*)&server, &len);
        if (n < 0)
        {
            LOG(FATAL, "recvfrom error!");
            exit(-1);
        }
        else
        {
            buffer[n] = 0;
            std::cerr << buffer << std::endl;
        }
    }
}

void send_func(int sockfd, uint16_t port, std::string ip, std::string& thread_name)
{
    while (true)
    {
        struct sockaddr_in sock;
        memset(&sock, 0, sizeof sock);
        sock.sin_family = AF_INET;
        sock.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &sock.sin_addr);

        std::string message;
        std::cout << "please input message: ";
        std::getline(std::cin, message);

        if (sendto(sockfd, message.c_str(), message.size(), 0, (sockaddr*)&sock, sizeof sock) < 0)
        {
            LOG(FATAL, "send to error");
            exit(-1);
        }

    }
}
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        LOG(FATAL, "USAGE -> %s IP PORT", argv[0]);
        exit(-1);
    }
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    uint16_t port = std::stoi(argv[2]);
    std::string ip = argv[1];

    if (sockfd < 0)
    {
        LOG(FATAL, "socket error!");
        exit(-1);
    }
    else
    {
        client_func send_func_bind = std::bind(send_func, sockfd, port, ip, std::placeholders::_1);
        client_func recv_func_bind = std::bind(receive_func, sockfd, std::placeholders::_1);
        Thread<client_func> recv_thread("recv_thread", recv_func_bind);
        Thread<client_func> send_thread("send_thread", send_func_bind);
 
        recv_thread.Start();
        send_thread.Start();

        recv_thread.Join();
        send_thread.Join();
    }
    return 0;
}
























// #include "Log.hpp"
// #include "InAddr.hpp"
// #include <iostream>
// #include <string>
// #include <memory>

// #include <cstring>

// #include <unistd.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>

// int main(int argc, char *argv[])
// {
//     if (argc != 3)
//     {
//         LOG(FATAL, "USAGE -> %s IP PORT", argv[0]);
//         exit(-1);
//     }
//     int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (sockfd < 0)
//     {
//         LOG(FATAL, "socket error!");
//         exit(-1);
//     }
//     else
//     {
//         while (true)
//         {
//             std::string message;
//             std::cout << "Please input message: ";
//             getline(std::cin, message);

//             struct sockaddr_in addr;
//             addr.sin_family = AF_INET;
//             addr.sin_port = htons(std::stoi(argv[2]));
//             addr.sin_addr.s_addr = inet_addr(argv[1]);

//             int ret = sendto(sockfd, message.c_str(), message.size(), 0, (sockaddr *)&addr, sizeof addr);
//             if (ret < 0)
//             {
//                 LOG(FATAL, "sendto error!");
//                 exit(-1);
//             }
//             else
//             {
//                 sockaddr_in tmp;
//                 socklen_t len = sizeof(tmp);
//                 LOG(DEBUG, "send success!");
//                 char buffer[1024];
//                 int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr *)&tmp, &len);
//                 if (n < 0)
//                 {
//                     LOG(FATAL, "recvfrom error!");
//                     exit(-1);
//                 }
//                 else
//                 {
//                     buffer[n] = '\0';
//                     LOG(DEBUG, "%s", buffer);
//                 }
//             }
//         }
//     }
//     return 0;
// }