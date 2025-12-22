#pragma once
#include "Log.hpp"
#include "Reactor.hpp"
#include <iostream>
#include <functional>

#define ListenConnection 0
#define NormalConnection 1

class Connection;
using handler_t = std::function<void(Connection*)>;

class Connection
{
public:
    Connection(int sockfd, int event, InAddr addr)
        :_sockfd(sockfd)
        ,_event(event)
        ,_addr(addr)
    {}

    int GetSockFD()
    {
        return _sockfd;
    }

    int GetEvent()
    {
        return _event;
    }

    void SetType(int type)
    {
        _type = type;
    }

    int GetType()
    {
        return _type;
    }

    InAddr GetAddr()
    {
        return _addr;
    }

    ~Connection()
    {}

private:
    int _sockfd;
    int _event;
    int _type;
    InAddr _addr;
    
public:
    handler_t send_func;
    handler_t recv_func;
    handler_t except_func;

    Reactor* _R;
};