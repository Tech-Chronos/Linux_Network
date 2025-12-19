#pragma once
#include "Log.hpp"
#include "Socket.hpp"

#include <iostream>
#include <memory>

#include <sys/epoll.h>

class EpollServer
{
private:
    const static int gdefault_size = 100;

public:
    EpollServer(uint16_t port)
        : _port(port), _listen_sock(std::make_shared<TcpSocket>()), _isrunning(false)
    {
        _listen_sock->TcpServerCreateSocket(_port);
    }

    void Init()
    {
        // 构造 epoll 模型
        _epfd = epoll_create(128);
        if (_epfd < 0)
        {
            LOG(ERROR, "epoll_create error!");
            exit(-1);
        }
        LOG(INFO, "epoll model create successfully!");

        // 在红黑树中插入节点
        epoll_event events;
        events.events = EPOLLIN;
        events.data.fd = _listen_sock->GetSockFd();
        epoll_ctl(_epfd, EPOLL_CTL_ADD, _listen_sock->GetSockFd(), &events);
    }

    void HandlerAccept()
    {
        // 有客户端链接
        // 1. 服务器accept
        InAddr addr;
        SockPtr conn_fd = _listen_sock->ServerAccept(&addr);

        LOG(INFO, "client info: %s:%d", addr.GetIP().c_str(), addr.GetPort());
        // 2. 将 connfd 放到 epoll 红黑树中
        epoll_event events;
        events.data.fd = conn_fd->GetSockFd();
        events.events = EPOLLIN;

        epoll_ctl(_epfd, EPOLL_CTL_ADD, conn_fd->GetSockFd(), &events);
        // 不能直接 recv，因为不知道客户端有没有发数据，recv 可能会阻塞，要交给 epoll
    }

    void HandlerIO(int sockfd)
    {
        char buffer[4096];
        int ret = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (ret > 0)
        {
            buffer[ret] = 0;
            LOG(INFO, "client say# %s", buffer);

            std::string header = "HTTP/1.0 200 OK\r\n";
            std::string content = "Hello Linux, Hello Network!";
            std::string type = "Content-Type: text/html\r\n";
            std::string length = "Content-Length: " + std::to_string(content.size()) + "\r\n";

            std::string echo_message = header + length + type + "\r\n" + content;
            send(sockfd, echo_message.c_str(), echo_message.size(), 0);
        }
        else if (ret == 0)
        {
            LOG(INFO, "client quit!");

            epoll_ctl(_epfd, EPOLL_CTL_DEL, sockfd, nullptr);
            close(sockfd);
        }
        else
        {
            LOG(ERROR, "recv error!");

            epoll_ctl(_epfd, EPOLL_CTL_DEL, sockfd, nullptr);
            close(sockfd);
        }
    }

    void HandlerEvents(int n)
    {
        for (int i = 0; i < n; ++i)
        {
            int sockfd = ready_events[i].data.fd;
            uint32_t event_type = ready_events[i].events;

            if (sockfd == _listen_sock->GetSockFd())
            {
                HandlerAccept();
            }
            else if (event_type & EPOLLIN)
            {
                HandlerIO(sockfd);
            }
        }
    }

    void Loop()
    {
        _isrunning = true;
        int max_time = 1000;
        while (_isrunning)
        {
            int ready_num = epoll_wait(_epfd, ready_events, gdefault_size, max_time);
            if (ready_num == 0)
            {
                LOG(INFO, "no events ready!");
                continue;
            }
            else if (ready_num < 0)
            {
                LOG(ERROR, "epoll_wait error!");
                exit(-1);
            }
            else
            {
                LOG(INFO, "had events ready!");
                for (int i = 0; i < ready_num; ++i)
                {
                    std::cout << "event fd is: " << ready_events[i].data.fd << " ";
                }
                std::cout << std::endl;
                HandlerEvents(ready_num);
            }
        }
    }

    ~EpollServer()
    {
    }

private:
    uint16_t _port;
    std::shared_ptr<TcpSocket> _listen_sock;
    bool _isrunning;

    // epoll
    int _epfd;
    epoll_event ready_events[gdefault_size];
};