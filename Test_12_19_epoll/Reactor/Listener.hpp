#pragma once
#include "Socket.hpp"
#include "Connection.hpp"
#include <sys/epoll.h>

class Reactor;
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
                LOG(INFO, "add %d to epoll!", conn_fd->GetSockFd());
            }
            else
            {
                if (code == EWOULDBLOCK)
                {
                    LOG(INFO, "已经全部链接完毕");
                    break;
                }
                else if (code == EINTR)
                {
                    LOG(INFO, "被信号中断");
                    continue;
                }
                else
                {
                    LOG(ERROR, "accept 错误！");
                    return;
                }
            }
        }
    }

    ~Listener()
    {
    }

private:
    SockPtr _listen_fd;
    int _port;

};