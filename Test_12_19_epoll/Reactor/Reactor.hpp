#pragma once
#include "Log.hpp"
#include "Socket.hpp"

#include <iostream>

class Reactor
{
public:
    Reactor(uint16_t port)
        :_port(port)
    {}

    void Init()
    {

    }

    ~Reactor()
    {}
private:
    uint16_t _port;
};