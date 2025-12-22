#pragma once
#include "Socket.hpp"
#include "Reactor.hpp"

class Listener
{
public:
    Listener(int port)
        : _listen_fd(std::make_shared<TcpSocket>()), _port(port)
    {
    }

    int Init()
    {
        _listen_fd->TcpServerCreateSocket(_port);

        return _listen_fd->GetSockFd();
    }

    void Accept(Connection* con)
    {
        InAddr addr;
        int code = 0;
        while (true)
        {
            SockPtr conn_fd = _listen_fd->ServerAccept(&addr, &code);
            if (conn_fd != nullptr)
            {
                con->_R->AddConnection(conn_fd->GetSockFd(), EPOLLIN | EPOLLET, NormalConnection, addr);
            }
            else
            {
                if (code == EWOULDBLOCK)
                    break;
                else if (code == EINTR)
                    continue;
                else
                    return;
            }
        }
    }

    ~Listener()
    {
    }

private:
    SockPtr _listen_fd;
    int _port;

    Reactor *_R;
};