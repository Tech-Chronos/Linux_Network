#pragma once
#include "Log.hpp"
#include "InAddr.hpp"
#include "Common.hpp"

#include <iostream>
#include <string>
#include <memory>

#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class BaseSocket;
using SockPtr = std::shared_ptr<BaseSocket>;

class BaseSocket
{
public:
    virtual void CreateSocket() = 0;
    virtual void ServerBind(uint16_t port) = 0;
    virtual void ServerListen() = 0;
    virtual SockPtr ServerAccept(InAddr* addr, int* code) = 0;

    virtual bool ClientConnect(std::string &ip, uint16_t port) = 0;

    virtual int RecvMessage(std::string *out) = 0;
    virtual int SenMessage(const std::string &message) = 0;

    virtual void Closefd() = 0;
    virtual void PortReuse() = 0;

    virtual int GetSockFd() = 0;

public:
    void TcpServerCreateSocket(uint16_t port)
    {
        CreateSocket();
        ServerBind(port);
        ServerListen();
        PortReuse();
    }

    bool TcpClientCreateSocket(std::string &ip, uint16_t port)
    {
        CreateSocket();
        return ClientConnect(ip, port);
    }
};

class TcpSocket : public BaseSocket
{
public:
    TcpSocket() = default;
    TcpSocket(int sockfd) : _sockfd(sockfd) {}

    void CreateSocket() override
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0)
        {
            LOG(FATAL, "socket error!");
            exit(-1);
        }
        SetNonBlock(_sockfd);
        LOG(INFO, "socket successfully, sockfd = %d", _sockfd);
    }

    void ServerBind(uint16_t port) override
    {
        sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        int ret = bind(_sockfd, (sockaddr *)&addr, sizeof(addr));
        if (ret < 0)
        {
            LOG(FATAL, "bind error!");
            exit(-1);
        }
        LOG(INFO, "bind successfully!");
    }

    void ServerListen() override
    {
        int ret = listen(_sockfd, 8);
        if (ret < 0)
        {
            LOG(FATAL, "listen error!");
            exit(-1);
        }
        LOG(INFO, "listen successfully!");
    }

    SockPtr ServerAccept(InAddr* addr, int* code) override
    {
        sockaddr_in client;
        socklen_t len = sizeof(client);
        int servicefd = accept(_sockfd, (sockaddr *)&client, &len);

        if (servicefd < 0)
        {
            LOG(ERROR, "accept error!");
            *code = errno;
            return nullptr;
        }

        SetNonBlock(servicefd);
        //InAddr client_addr(client);
        *addr = client;
        return std::make_shared<TcpSocket>(servicefd);
    }

    bool ClientConnect(std::string &ip, uint16_t port) override
    {
        sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &server.sin_addr);

        int ret = connect(_sockfd, (sockaddr *)&server, sizeof(server));

        if (ret < 0)
        {
            LOG(ERROR, "connect error!");
            return false;
        }

        return true;
    }

    int RecvMessage(std::string *out) override
    {
        char buffer[4096];

        int n = recv(_sockfd, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0)
        {
            LOG(ERROR, "recv error!");
            return 0;
        }
        else
        {
            buffer[n] = 0;
            *out += buffer;
        }
        return n;
    }

    int SenMessage(const std::string &message) override
    {
        int n = send(_sockfd, message.c_str(), message.size(), 0);
        if (n < 0)
        {
            LOG(ERROR, "send error!");
            return n;
        }
        return n;
    }

    void Closefd() override
    {
        if (_sockfd > 0)
            close(_sockfd);
    }

    void PortReuse() override
    {
        int opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    int GetSockFd() override
    {
        return _sockfd;
    }

    ~TcpSocket()
    {
    }

private:
    int _sockfd;
};
