#pragma once
#include "Log.hpp"
#include "InAddr.hpp"
#include "ThreadPool.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <functional>

#include <cstring>

using func_t = std::function<void(InAddr, int, std::string)>;


class User
{
public:
    User(const InAddr& addr)
        :_addr(addr)
        //,_name("user")
    {}

    InAddr GetInetAddr() const
    {
        return _addr;
    }
private:
    InAddr _addr;
    //std::string _name;
};

class Group
{
public:
    Group()
    {}

    void Check(const User& newuser)
    {
        for (auto& user : _users)
        {
            if (newuser.GetInetAddr() == user.GetInetAddr())
            {
                return;
            }
        }
        _users.push_back(newuser);
    }

    void OffLine(const User& olduser)
    {
        std::vector<User>::iterator it = _users.begin();
        while (it != _users.end())
        {
            if (it->GetInetAddr() == olduser.GetInetAddr())
                break;
            ++it;
        }
        if (it != _users.end())
            _users.erase(it);
    }

    void Forward(InAddr addr, int sockfd, std::string message, std::string& thread_name)
    {
        std::string ip = addr.GetIP();
        std::string port = std::to_string(addr.GetPort());

        std::string full_message = "[" + ip + ": " + port + "]# " + message; 
        for (auto& user : _users)
        {
            sockaddr_in guy;
            guy = user.GetInetAddr().Addr();

            sendto(sockfd, full_message.c_str(), full_message.size(), 0, (struct sockaddr*)&guy, sizeof(guy));
        }
    }

    void SendMessageToAll(InAddr addr, int sockfd, std::string message)
    {
        Check(User(addr));

        if (message == "q")
        {
            OffLine(User(addr));
        }

        task_t task_thread = std::bind(&Group::Forward, this, addr, sockfd, message, std::placeholders::_1);

        ThreadPool<task_t>::GetSingleInstance()->Enqueue(task_thread);
        
    }

private:
    std::vector<User> _users;
};