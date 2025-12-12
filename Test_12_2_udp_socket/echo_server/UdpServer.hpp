#include "Log.hpp"
#include "InetAddr.hpp"

#include <iostream>
#include <string>
#include <memory>

#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Log lg;

uint16_t gport = 8888;
//std::string gip = "127.0.0.1";

class UdpServer
{
public:
    UdpServer(uint16_t port = gport)
        : _port(port), _isrunning(false)
    {
    }

    void Init()
    {
        // 1. socket,可以理解为打开网卡
        _socketfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (_socketfd < 0)
        {
            LOG(FATAL, "socket error!");
            exit(-1);
        }
        else
        {
            LOG(DEBUG, "socket fd = %d", _socketfd);
        }

        // 2. socket绑定 IP & PORT
        struct sockaddr_in addr;
        // 2.1 先清空
        bzero(&addr, sizeof(addr));

        // 2.2 设置 IPV4
        addr.sin_family = AF_INET;

        // 2.3 设置端口
        // 网络传输默认是需要大端的字节序
        // 将主机字节序转换成网络字节序
        // (_port << 8) | (_port >> 8)即可;
        addr.sin_port = htons(_port);

        // 2.4 设置 IP 地址
        // 将ip从点分十进制的字符串形式转换成一个四字节的整数
        // 将这个整数转换成网络所需要的大端的字节序
        //addr.sin_addr.s_addr = inet_addr(_ip.c_str());

        // 这个表示可以接受任意的
        addr.sin_addr.s_addr = htons(INADDR_ANY);

        // 3. 绑定到socket
        int ret = bind(_socketfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        if (ret == 0)
        {
            LOG(DEBUG, "bind success");
        }
        else
        {
            LOG(FATAL, "bind failture");
            exit(-1);
        }
    }

    void Start()
    {
        _isrunning = true;
        while (_isrunning)
        {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            struct sockaddr_in peer_addr;
            socklen_t peer_len = sizeof(peer_addr);
            int n = recvfrom(_socketfd, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr *>(&peer_addr), &peer_len);
            if (n > 0)
            {
                buffer[n] = '\0';

                InetAddr ia(peer_addr);

                uint16_t hport = ia.GetHostPort();
                std::string hip = ia.GetHostIP();

                std::string recv_message = buffer;
                // 打印收到的客户端的数据
                LOG(DEBUG, "recv: %s from %s:%d",
                    recv_message.c_str(),
                    hip.c_str(),
                    hport);

                std::string echo_message = "sever echo# " + recv_message;
                int ret = sendto(_socketfd, echo_message.c_str(), echo_message.size(), 0, (sockaddr *)&peer_addr, peer_len);
                if (ret < 0)
                {
                    LOG(FATAL, "sendto error!");
                    exit(-1);
                }
                else
                {
                    LOG(DEBUG, "send to client successfully!");
                }
            }
            else
            {
                LOG(FATAL, "receive error!");
                exit(-1);
            }
        }
    }

    ~UdpServer()
    {
        if (_isrunning)
            close(_socketfd);
    }

private:
    int _socketfd;
    uint16_t _port;
    //std::string _ip;
    bool _isrunning;
};