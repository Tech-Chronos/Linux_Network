#pragma once
#include <iostream>
#include <string>

#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class InAddr
{
private:
    void ToHost()
    {
        _port = ntohs(_addr.sin_port);
        _ip = inet_ntoa(_addr.sin_addr);
    }
public:
    InAddr(const struct sockaddr_in& addr)
        :_addr(addr)
    {
        ToHost();
    }

    std::string GetIP()
    {
        return _ip;
    }

    uint16_t GetPort()
    {
        return _port;
    }
    
    ~InAddr()
    {}

private:
    struct sockaddr_in _addr;
    uint16_t _port;
    std::string _ip;
};