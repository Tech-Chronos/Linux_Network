#pragma once
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

class InetAddr
{
private:
    void IP_NetToHost()
    {
        _ip = inet_ntoa(_addr.sin_addr);
    }

    void Port_NetToHost()
    {
        _port = ntohs(_addr.sin_port);
    }

public:
    InetAddr(const struct sockaddr_in& addr)
        :_addr(addr)
    {}

    std::string GetHostIP()
    {
        IP_NetToHost();
        return _ip;
    }

    uint16_t GetHostPort()
    {
        Port_NetToHost();
        return _port;
    }

private:
    struct sockaddr_in _addr;
    std::string _ip;
    uint16_t _port;
};