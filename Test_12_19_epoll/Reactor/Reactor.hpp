#pragma once
#include "Connection.hpp"
#include "Multiplexing.hpp"
#include "HandlerConnection.hpp"

#include <unordered_map>
#include <iostream>
#include <memory>


class Listener;
class Connection;

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

        // 1.2 设置 this 指针到 Connection 中
        con->SetReactor(this);

        // 2. 将 con 插入到connects
        _connects[sockfd] = con;

        // 3. 将新的 con 让 epoll 管理
        if (_epoll_ptr->AddEvent(con->GetSockFD(), con->GetEvent()))
        {
            LOG(INFO, "epoll add success, sockfd = %d, event = %s", con->GetSockFD(), EventToString(event).c_str());
            // 如果是 监听 套接字 设置回调函数
            if (con->GetType() == ListenConnection)
            {
                con->RegisterHandler(_OnConnect, nullptr, nullptr);
            }
            // 如果是 普通 套接字 设置回调函数
            else if (con->GetType() == NormalConnection)
            {
                con->RegisterHandler(_OnRecver, _OnSender, _OnExcepter);
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

    void SetOnConnect(handler_t OnConnect)
    {
        _OnConnect = OnConnect;
    }
    void SetOnNormalHandler(handler_t recver, handler_t sender, handler_t excepter)
    {
        _OnRecver = recver;
        _OnSender = sender;
        _OnExcepter = excepter;
    }

    ~Reactor()
    {
    }

private:
    std::unordered_map<int, Connection *> _connects;
    std::shared_ptr<Multiplex> _epoll_ptr;

    epoll_event revent[gsize];
    bool _isrunning;

    // Reactor中添加处理socket的方法集合
    // 1. 处理新连接
    handler_t _OnConnect;
    // 2. 处理普通的sockfd，主要是IO处理
    handler_t _OnRecver;
    handler_t _OnSender;
    handler_t _OnExcepter;
};