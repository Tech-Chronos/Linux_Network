#pragma once
#include "InAddr.hpp"
#include <iostream>
#include <functional>

#define ListenConnection 0
#define NormalConnection 1

class Reactor;
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

    void RegisterHandler(handler_t recver, handler_t sender, handler_t excepter)
    {
        recv_func = recver;
        send_func = sender;
        except_func = excepter;
    }

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

    void SetReactor(Reactor* R)
    {
        _R = R;
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