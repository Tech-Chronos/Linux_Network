#pragma once
#include "Log.hpp"
#include "Socket.hpp"

#include <pthread.h>

using func_t = std::function<std::string(std::string &)>;

class TcpServer;
struct ThreadData
{
public:
    ThreadData(TcpServer *server, SockPtr service_fd)
        : _server(server), _service_fd(service_fd)
    {
    }

public:
    TcpServer *_server;
    SockPtr _service_fd;
};

class TcpServer
{
public:
    TcpServer(func_t func, uint16_t port = 8888)
        : _func(func), _isrunning(false)
    {
        _sock = std::make_shared<TcpSocket>();
        _sock->TcpServerCreateSocket(port);
    }

    void Loop()
    {
        _isrunning = true;
        while (_isrunning)
        {
            SockPtr service_fd = _sock->ServerAccept();

            pthread_t tid;
            ThreadData *td = new ThreadData(this, service_fd);
            pthread_create(&tid, nullptr, ThreadFunc, td);
        }
    }

    ~TcpServer()
    {
    }

private:
    static void *ThreadFunc(void *args)
    {
        pthread_detach(pthread_self());

        ThreadData *td = static_cast<ThreadData *>(args);
        TcpServer *self = td->_server;

        std::string request;
        int ret = td->_service_fd->RecvMessage(&request);

        if (ret > 0)
        {
            //LOG(INFO, "\nrequest: %s", request.c_str());
            // 处理请求
            std::string resp = self->_func(request);

            // 发送回复
            td->_service_fd->SenMessage(resp);
        }

        td->_service_fd->Closefd();
        delete td;
        return nullptr;
    }

private:
    SockPtr _sock;
    func_t _func;
    bool _isrunning;
};
