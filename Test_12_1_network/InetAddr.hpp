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
    InetAddr(const sockaddr_in& addr)
        :_addr(addr)
    {}

    std::string GetHostIP()
    {
        IP_NetToHost();
        return _ip;
    }

    std::string GetHostPort()
    {
        Port_NetToHost();
        return _port;
    }

private:
    sockaddr_in _addr;
    std::string _ip;
    std::string _port;
};