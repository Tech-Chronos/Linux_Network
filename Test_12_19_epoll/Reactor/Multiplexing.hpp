#include "Log.hpp"

#include <sys/epoll.h>

static const int gnum = 128;
class Multiplex
{
public:
    virtual bool AddEvent(int sockfd, int ev) = 0;
    virtual int Wait(epoll_event revent[], int maxevent, int timeout = -1) = 0;
};

class Epoller : public Multiplex
{
public:
    Epoller()
    {
        _epfd = epoll_create(gnum);
    }

    bool AddEvent(int sockfd, int ev) override
    {
        epoll_event event;
        event.data.fd = sockfd;
        event.events = ev;
        int ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, sockfd, &event);
        if (ret == 0) return true;
        else return false;
    }

    int Wait(epoll_event revent[], int maxevent, int timeout)
    {
        int ret = epoll_wait(_epfd, revent, maxevent, timeout);
        if (ret > 0)
        {
            LOG(INFO, "epoll_wait success, prepared num is %d", ret);
        }
        else if (ret == 0)
        {
            LOG(INFO, "no event prepared!");
        }
        else
        {
            LOG(ERROR, "epoll wait error!");
        }
        return ret;
    }

private:
    int _epfd;
};