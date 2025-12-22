#pragma once
#include "Connection.hpp"

const static int bufsize = 1024;
class HandlerIO
{
public:
    void RecvMessage(Connection* con)
    {
        LOG(INFO,"recv message from %s:%d!",con->GetAddr().GetIP().c_str(), con->GetAddr().GetPort());

        // char buffer[bufsize];
        // while (true)
        // {
        //     int ret = 
        // }
        
    }
    void SendMessage(Connection* con)
    {

    }
    void HandleExcept(Connection* con)
    {

    }
};