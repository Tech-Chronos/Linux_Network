#pragma once
#include "Log.hpp"
#include "Socket.hpp"
#include "Connection.hpp"
#include "Multiplexing.hpp"
#include "Listener.hpp"
#include "HandlerConnection.hpp"

#include <unordered_map>

#include <iostream>

const static int gsize = 128;

class Reactor
{
public:
    Reactor()
        : _epoll_ptr(std::make_shared<Epoller>()), _isrunning(false)
    {
    }

    std::string EventToString(int event)
    {
        std::string epoll_str;
        if (event & EPOLLIN)
            epoll_str += "EPOLLIN";
        if (event & EPOLLOUT)
            epoll_str += "|EPOLLOUT";
        if (event & EPOLLET)
            epoll_str += "|EPOLLET";
        return epoll_str;
    }

    void AddConnection(int sockfd, int event, int conn_type, InAddr addr)
    {
        // 1. 构建 connection
        Connection *con = new Connection(sockfd, event, addr);

        // 1.1 设置套接字类型 type
        con->SetType(conn_type);

        // 2. 将 con 插入到connects
        _connects[sockfd] = con;

        // 3. 将新的 con 让 epoll 管理
        if (_epoll_ptr->AddEvent(con->GetSockFD(), con->GetEvent()))
        {
            LOG(INFO, "epoll add success, sockfd = %d, event = %s", con->GetSockFD(), EventToString(event).c_str());
            // 如果是 监听 套接字 设置回调函数
            if (con->GetType() == ListenConnection)
            {
                con->recv_func = std::bind(&Listener::Accept, Listener(con->GetAddr().GetPort()), std::placeholders::_1);
                con->send_func = nullptr;
                con->except_func = nullptr;
            }
            // 如果是 普通 套接字 设置回调函数
            else if (con->GetType() == NormalConnection)
            {
                con->recv_func = std::bind(&HandlerIO::RecvMessage, HandlerIO(), std::placeholders::_1);
                con->send_func = std::bind(&HandlerIO::SendMessage, HandlerIO(), std::placeholders::_1);
                con->except_func = std::bind(&HandlerIO::HandleExcept, HandlerIO(), std::placeholders::_1);
            }
        }
    }

    void LoopOnce(int i)
    {
        int sockfd = revent[i].data.fd;
        int ev = revent[i].events;
        LOG(DEBUG, "had event prepared! sockfd = %d", revent[i].data.fd);
        if (ev == EPOLLERR)
            ev |= (EPOLLIN | EPOLLET);

        if (ev == EPOLLHUP)
            ev |= (EPOLLIN | EPOLLET);

        if (ev == EPOLLIN)
        {
            if (WheatherExistCon(sockfd) && _connects[sockfd]->recv_func)
                _connects[sockfd]->recv_func(_connects[sockfd]);
        }
        else if (ev == EPOLLOUT)
        {
            if (WheatherExistCon(sockfd) && _connects[sockfd]->recv_func)
                _connects[sockfd]->send_func(_connects[sockfd]);
        }
        else
        {
            if (WheatherExistCon(sockfd) && _connects[sockfd]->recv_func)
                _connects[sockfd]->except_func(_connects[sockfd]);
        }
    }
    
    void Dispatch()
    {
        _isrunning = true;
        while (_isrunning)
        {
            int ret = _epoll_ptr->Wait(revent, gsize);
            if (ret > 0)
            {
                for (int i = 0; i < ret; ++i)
                {
                    LoopOnce(i);
                }
            }
            else if (ret == 0)
            {
                continue;
            }
            else
            {
                return;
            }
        }
        _isrunning = false;
    }

    bool WheatherExistCon(int sockfd)
    {
        auto it = _connects.find(sockfd);
        if (it == _connects.end())
            return false;

        return true;
    }

    ~Reactor()
    {
    }

private:
    std::unordered_map<int, Connection *> _connects;
    std::shared_ptr<Multiplex> _epoll_ptr;

    epoll_event revent[gsize];
    bool _isrunning;
};