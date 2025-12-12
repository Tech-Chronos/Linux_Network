#pragma once
#include "Log.hpp"
#include "InAddr.hpp"
#include "User.hpp"
#include <iostream>
#include <string>
#include <memory>
#include <functional>

#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


class UdpServer
{
public:
    UdpServer(uint16_t port, func_t func)
        :_port(port)
        ,_isrunning(false)
        ,_func(func)
    {
    }

    void Init()
    {
        _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (_sockfd < 0)
        {
            LOG(FATAL, "socket error!");
            exit(-1);
        }
        else
        {
            sockaddr_in addr;
            memset(&addr, 0, sizeof addr);

            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htons(INADDR_ANY);
            addr.sin_port = htons(_port);
            
            int ret = bind(_sockfd, (sockaddr*)&addr, sizeof addr);
            if (ret != 0)
            {
                LOG(FATAL, "bind error!");
                exit(-1);
            }
            else
            {
                LOG(DEBUG, "bind successfully, sockfd = %d", _sockfd);
            }
        }
    }

    void Start()
    {
        _isrunning = true;
        while (_isrunning)
        {
            LOG(DEBUG, "waiting for message!");
            sockaddr_in peer_addr;
            socklen_t peer_len = sizeof peer_addr;

            char buffer[1024];
            int n = recvfrom(_sockfd, buffer, sizeof(buffer) - 1, 0 , (sockaddr*)&peer_addr, &peer_len);
            if (n < 0)
            {
                LOG(FATAL, "recvfrom error!");
                exit(-1);
            }
            else
            {
                buffer[n] = '\0';

                std::string message = buffer;
                InAddr addr(peer_addr);

                LOG(DEBUG, "receive from %s:%d say: %s", addr.GetIP().c_str(), addr.GetPort(), message.c_str());

                _func(addr, _sockfd, message);
            }
        }
    }

    ~UdpServer()
    {
        if (_isrunning)
            close(_sockfd);
    }

private:
    int _sockfd;
    uint16_t _port;
    bool _isrunning;

    func_t _func;
};