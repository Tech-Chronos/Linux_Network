#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>

#include <cassert>
#include <cstdint>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#define INF 0
#define DBG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL INF

#define LOG(level, format, ...)                                                                                          \
    {                                                                                                                    \
        if (level >= DEFAULT_LOG_LEVEL)                                                                                  \
        {                                                                                                                \
            time_t t = time(nullptr);                                                                                    \
            struct tm *m = localtime(&t);                                                                                \
            char ch[32] = {0};                                                                                           \
            strftime(ch, 31, "%H:%M:%S", m);                                                                             \
            fprintf(stdout, "[%p %s %s:%d]" format "\n", (void *)pthread_self(), ch, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                                                                                \
    }
#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#define DEFAULT_BUFFER_SIZE 512
#define DEFAULT_BACKLOG 4
#define MAX_EPOLLEV_NUM 1024
#define DEFAULT_TASK_POOL 32
#define DEFAULT_WHEEL_NUM 128

using EventCallBack = std::function<void()>;
using Functor = std::function<void()>;
using OnTimerCallBack = std::function<void()>;
using ReleaseCallBack = std::function<void()>;

class TimerTask;
using PtrTimerTask = std::shared_ptr<TimerTask>;
using WeakTimerTask = std::weak_ptr<TimerTask>;

class Buffer
{
public:
    Buffer()
        : _buffer(512), _read_idx(0), _write_idx(0)
    {
    }

    // 1. 获取首元素的地址
    char *Begin()
    {
        return &_buffer[0];
    }

    // 获取读位置
    char *ReadPosition()
    {
        return Begin() + _read_idx;
    }

    // 获取写位置
    char *WritePosition()
    {
        return Begin() + _write_idx;
    }

    // 2. 获取前沿空间的大小
    int GetFrontSpace()
    {
        return _buffer.size() - _write_idx;
    }

    // 3. 获取后沿空间大小
    int GetBackSpace()
    {
        return _read_idx;
    }

    // 4. 获取可读元素的数量
    int GetReadableNum()
    {
        return _write_idx - _read_idx;
    }

    // 5. 将读偏移向后移
    void MoveReadOffset(int len)
    {
        assert(len <= GetReadableNum());
        _read_idx += len;
    }

    // 6. 将写偏移向后移
    void MoveWriteOffset(int len)
    {
        assert(len <= GetBackSpace());
        _write_idx += len;
    }

    // 7. 确保可写空间足够(判断剩余空间是否足够，不够就扩容)
    void EnsureAmpleSpace(int len)
    {
        if (len <= GetFrontSpace())
            return;
        else if (len <= GetBackSpace() + GetFrontSpace())
        {
            int readable = GetReadableNum();
            std::copy(_buffer.begin() + _read_idx, _buffer.begin() + _write_idx, _buffer.begin());
            _read_idx = 0;
            _write_idx += readable;
            return;
        }
        else
        {
            _buffer.resize(len);
            return;
        }
    }

    /// 8. 写数据
    void Write(const char *data, int len)
    {
        // 首先判断有足够的写空间
        EnsureAmpleSpace(len);
        std::copy(data, data + len, _buffer.begin() + _write_idx);
    }

    void WriteAndPush(const char *data, int len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }

    // 写字符串
    void WriteString(const std::string &data)
    {
        Write(data.c_str(), data.size());
    }

    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }

    // 写缓冲区
    void WriteBuffer(Buffer &buf)
    {
        Write(buf.ReadPosition(), buf.GetReadableNum());
    }

    void WriteBufferAndPush(Buffer &buf)
    {
        WriteBuffer(buf);
        MoveWriteOffset(buf.GetReadableNum());
    }

    // 9. 读数据
    void Read(char *space, int len)
    {
        assert(len <= GetReadableNum());
        std::copy(ReadPosition(), WritePosition(), space);
    }

    void ReadAndPop(char *space, int len)
    {
        Read(space, len);
        MoveReadOffset(len);
    }

    std::string ReadAsString(uint64_t len)
    {
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }

    std::string ReadAsStringAndPop(uint64_t len)
    {
        std::string ret = ReadAsString(len);
        MoveReadOffset(len);
        return ret;
    }

    char *FindCRLF()
    {
        char *pos = (char *)memchr(ReadPosition(), '\n', GetReadableNum());

        return pos;
    }

    // 10. 获取一行元素
    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (!pos)
            return "";
        std::string str = ReadAsString(pos - &_buffer[_read_idx] + 1);
        return str;
    }

    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        return str;
    }

    // 清空缓冲区
    void Clear()
    {
        _buffer.clear();
        _read_idx = _write_idx = 0;
    }

private:
    std::vector<char> _buffer;
    int _read_idx;
    int _write_idx;
};

class Socket
{
public:
    Socket(int sockfd)
        : _sockfd(sockfd)
    {
    }

    Socket()
    {
    }

    bool CreateSocket()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0)
        {
            ERR_LOG("create socket error!");
            return false;
        }
        return true;
    }

    bool Bind(uint16_t port, const std::string &ip)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr.s_addr);

        socklen_t len = sizeof(addr);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);

        if (ret < 0)
        {
            ERR_LOG("Bind Error!");
            return false;
        }
        return true;
    }

    bool Listen()
    {
        int ret = listen(_sockfd, DEFAULT_BACKLOG);
        if (ret < 0)
        {
            ERR_LOG("Listen ERROR");
            return false;
        }
        return true;
    }

    int Accept()
    {
        int io_fd = accept(_sockfd, nullptr, nullptr);
        if (io_fd < 0)
        {
            ERR_LOG("accept error!");
            return false;
        }
        return io_fd;
    }

    bool Connection(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr.s_addr);

        int ret = connect(_sockfd, (struct sockaddr *)&addr, sizeof(sockaddr_in));
        if (ret < 0)
        {
            ERR_LOG("CONNECTION ERROR!");
            return false;
        }
        return true;
    }

    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        int ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            // 在非阻塞的情况下，EAGAIN表示没有数据了，EINTR表示被信号中断了
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("RECV ERROR!");
            return -1;
        }
        return ret;
    }

    // 非阻塞接收
    ssize_t RecvNonBlock(void *buf, size_t len)
    {
        ssize_t ret = Recv(buf, len, MSG_DONTWAIT);
        return ret;
    }

    ssize_t Send(void *buf, size_t len, int flag = 0)
    {
        int ret = send(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SEND ERROR!");
            return -1;
        }
        return ret;
    }

    ssize_t SendNonBlock(void *buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT);
    }

    void Close()
    {
        if (_sockfd > 0)
            close(_sockfd);
    }

    // ip一般绑定所有的网卡
    int CreateServer(uint16_t port, const std::string &ip = INADDR_ANY, int block_flag = false)
    {
        if (CreateSocket() == false)
            return false;
        // 设置非组素
        if (block_flag)
            SetNonBlock();
        // 重复使用IP + port
        ReuseAddr();
        if (Bind(port, ip) == false)
            return false;
        if (Listen() == false)
            return false;
        return _sockfd;
    }

    // 客户端不要绑定自己的端口
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        if (CreateSocket() == false)
            return false;
        if (Connection(ip, port) == false)
            return false;
        return true;
    }

    void ReuseAddr()
    {
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
    }

    void SetNonBlock()
    {
        int ret = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, ret | O_NONBLOCK);
    }

    ~Socket()
    {
    }

private:
    int _sockfd;
};

class Poller;
class EventLoop;
class Channel
{
public:
    void Update();

    void Remove();

public:
    Channel(int fd, EventLoop *loop)
        : _fd(fd), _events(0), _revents(0), _loop(loop)
    {
    }

    // 设置回调函数
    void SetAnyCallBack(const EventCallBack &cb)
    {
        _any_cb = cb;
    }

    void SetReadCallBack(const EventCallBack &cb)
    {
        _read_cb = cb;
    }

    void SetWriteCallBack(const EventCallBack &cb)
    {
        _write_cb = cb;
    }

    void SetErrorCallBack(const EventCallBack &cb)
    {
        _error_cb = cb;
    }

    void SetCloseCallBack(const EventCallBack &cb)
    {
        _close_cb = cb;
    }

    // 处理事件
    void HandleEvents()
    {
        if (_any_cb)
            _any_cb();
        if (_revents & (EPOLLIN | EPOLLRDHUP | EPOLLPRI))
        {
            if (_read_cb)
                _read_cb();
        }
        else if (_revents & EPOLLOUT)
        {
            if (_write_cb)
                _write_cb();
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_cb)
                _error_cb();
        }
        else if (_revents & EPOLLHUP)
        {
            if (_close_cb)
                _close_cb();
        }
    }

    // 文件描述符是否可读 or 可写
    bool ReadAble()
    {
        return _events & EPOLLIN;
    }

    bool WriteAble()
    {
        return _events & EPOLLOUT;
    }

    // 设置文件描述符可读 可写 and 不可读 不可写 and 清空所有关心的事件
    void EnableRead()
    {
        _events & EPOLLIN;
        Update();
    }

    void EnableWrite()
    {
        _events & EPOLLOUT;
        Update();
    }

    void DisableRead()
    {
        _events & ~EPOLLIN;
        Update();
    }

    void DisableWrite()
    {
        _events & ~EPOLLOUT;
        Update();
    }

    void DisableAll()
    {
        _events = 0;
        Update();
    }

    // 获取文件描述符
    int Fd()
    {
        return _fd;
    }

    // 获取关心的事件
    int GetCareEvents()
    {
        return _events;
    }

    // 设置文件描述符已经就绪的事件
    void SetRevents(int revents)
    {
        _revents = revents;
    }

private:
    int _fd;
    int _events;  // 关心的事件
    int _revents; // 这个文件描述符触发的事件
    // Poller* _poller;
    EventLoop *_loop;

    EventCallBack _any_cb;
    EventCallBack _read_cb;
    EventCallBack _write_cb;
    EventCallBack _error_cb;
    EventCallBack _close_cb;
};

class Poller
{
private:
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it == _channels.end())
            return false;
        return true;
    }

    void Update(Channel *channel, int op)
    {
        struct epoll_event ev;
        ev.data.fd = channel->Fd();
        ev.events = channel->GetCareEvents();

        int ret = epoll_ctl(_epfd, op, channel->Fd(), &ev);

        if (ret < 0)
        {
            ERR_LOG("EPOLL CTL ERROR!");
            exit(-1);
        }
    }

public:
    Poller()
    {
        _epfd = epoll_create(1024);
        if (_epfd < 0)
        {
            ERR_LOG("EPOLL CREATE ERROR!");
            exit(-1);
        }
    }
    // 增加或修改文件描述符的事件
    void UpdateEvent(Channel *channel)
    {
        if (!HasChannel(channel))
        {
            Update(channel, EPOLL_CTL_ADD);
            _channels.insert(std::make_pair(channel->Fd(), channel));
        }
        Update(channel, EPOLL_CTL_MOD);
    }

    // 删除文件描述符的事件
    void RemoveEvent(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it != _channels.end())
        {
            _channels.erase(channel->Fd());
        }
        Update(channel, EPOLL_CTL_DEL);
    }

    void Poll(std::vector<Channel *> *active)
    {
        // 阻塞等待
        int ready = epoll_wait(_epfd, _ev, MAX_EPOLLEV_NUM, -1);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("EPOLL WAIT ERROR!");
            exit(-1);
        }
        if (ready > 0)
        {
            for (int i = 0; i < ready; ++i)
            {
                // 获取就绪的文件描述符和这个文件描述符的事件
                int fd = _ev[i].data.fd;
                int revents = _ev[i].events;

                auto it = _channels.find(fd);
                if (it != _channels.end())
                {
                    it->second->SetRevents(revents);
                    active->push_back(it->second);
                }
            }
        }
    }

private:
    int _epfd;
    struct epoll_event _ev[MAX_EPOLLEV_NUM];      // 存储就绪的事件(文件描述符和就绪的事件)
    std::unordered_map<int, Channel *> _channels; // 用哈希表管理当前的 POLLER 中是否存在关心的fd
};

class TimerTask
{
public:
    TimerTask(int timerid, int timeout)
        : _timer_id(timerid), _timeout(timeout), _canceled(false)
    {
    }

    void SetOnTimeCallBack(const OnTimerCallBack &cb)
    {
        _timer_cb = cb;
    }

    void SetReleaseCallBack(const ReleaseCallBack &cb)
    {
        _release_cb = cb;
    }

    void CancelOnTimeCallBack()
    {
        _canceled = false;
    }

    uint64_t TimerId()
    {
        return _timer_id;
    }

    // 当外部需要刷新时间轮的时候，需要获取超时时间，重新插入
    int TimeOut()
    {
        return _timeout;
    }

    ~TimerTask()
    {
        if (!_canceled)
        {
            if (_timer_cb)
                _timer_cb();
        }
        if (_release_cb)
            _release_cb();
    }

private:
    uint64_t _timer_id;
    int _timeout;
    bool _canceled;
    OnTimerCallBack _timer_cb;   // 定时任务
    ReleaseCallBack _release_cb; // 需要从TimerWheel中移除管理的回调函数
};

class TimerWheel
{
private:
    void RemoveWeakTimerFromWheel(uint64_t timerid)
    {
        auto it = _timers.find(timerid);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }

    int CreateTimerFd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("TIMERFD CREATE ERROR!");
            exit(-1);
        }

        // 第一次触发事件是1s之后，之后每隔1s触发一次
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0;

        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0;

        // 用settime设置
        timerfd_settime(_timerfd, 0, &itime, nullptr);
        return timerfd;
    }

    void ReadTime()
    {
        uint64_t val = 0;
        int ret = read(_timerfd, &val, sizeof(val));
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return;
            }
            ERR_LOG("READ TIME ERROR!");
            exit(-1);
        }
    }

    void OnTimeCallBack()
    {
        ReadTime();
        RunTimerTask();
    }

private:
    // 出了这个作用域 shared_ptr 的强引用计数变成了 1
    void AddTimerTaskInloop(uint64_t timerid, int timeout, const OnTimerCallBack &on_time_task)
    {
        if (timeout > _capacity || timeout < 0)
            return;

        // 构造TimerTask的智能指针
        PtrTimerTask timer = std::make_shared<TimerTask>(timerid, timeout);

        // 设置定时处理函数
        timer->SetOnTimeCallBack(on_time_task);
        // 从当前的哈希表中移除的回调函数
        timer->SetReleaseCallBack(std::bind(&TimerWheel::RemoveWeakTimerFromWheel, this, timerid));

        // 放到时间轮中
        _wheels[(_tick + timeout) % _capacity].push_back(timer);

        // 放到自己管理的哈希表中
        _timers[timerid] = WeakTimerTask(timer);
    }

    void RefreshTimerTaskInLoop(uint64_t timeid)
    {
        auto it = _timers.find(timeid);
        if (it != _timers.end())
        {
            PtrTimerTask timer = it->second.lock();
            _wheels[(_tick + timer->TimeOut()) % _capacity].push_back(timer);
        }
    }

    void CancelTimerInLoop(uint64_t timeid)
    {
        auto it = _timers.find(timeid);
        if (it != _timers.end())
        {
            PtrTimerTask timer = it->second.lock();
            timer->CancelOnTimeCallBack();
        }
    }

    void RunTimerTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheels[_tick].clear();
    }

public:
    TimerWheel(EventLoop *loop)
        : _tick(0), _capacity(DEFAULT_WHEEL_NUM), _wheels(_capacity), _timerfd(CreateTimerFd()), _timer_channel(_timerfd, _loop), _loop(loop)
    {
        _timer_channel.SetReadCallBack(std::bind(&TimerWheel::OnTimeCallBack, this));
        _timer_channel.EnableRead();
    }

    // 防止在业务线程中多线程对_timers和_wheels访问，造成多线程并发访问的问题，所以统一在loop自己的线程中运行
    void AddTimerTask(uint64_t timerid, int timeout, const OnTimerCallBack &on_time_task);
    void RefreshTimerTask(uint64_t timeid);
    void CancelTimerTask(uint64_t timeid);

    bool HasTimer(uint64_t timerid)
    {
        auto it = _timers.find(timerid);
        if (it != _timers.end())
        {
            return true;
        }
        return false;
    }

private:
    int _tick;
    std::vector<std::vector<PtrTimerTask>> _wheels;
    int _capacity;
    std::unordered_map<uint64_t, WeakTimerTask> _timers;

    int _timerfd;
    Channel _timer_channel;
    EventLoop *_loop;
};

// 只关心调度，不关心事件是怎么被处理的
class EventLoop
{
private:
    void RunAllTask()
    {
        // 为了防止其他线程在任务池中插入任务，导致多线程并发访问的问题，所以要加锁
        // 加锁之后为了防止这个线程占用太长时间的任务池，所以只需要交换一下内容，在锁的外部执行
        std::vector<Functor> task_pool;
        {
            std::lock_guard lock(_mutex);
            _task_pool.swap(task_pool);
        }

        for (auto &func : task_pool)
        {
            func();
        }
    }

    int CreateEventFd()
    {
        int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (fd < 0)
        {
            ERR_LOG("EVENTFD ERROR!");
            exit(-1);
        }
        return fd;
    }

    // 就是在eventfd中写入数据
    void WakeUpEventFd()
    {
        uint64_t val = 1;
        int ret = write(_eventfd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR)
                return;
            ERR_LOG("WakeUpEventFd ERROR!");
            exit(-1);
        }
    }

    // eventfd事件就绪，执行的可读回调函数
    void ReadEventFd()
    {
        uint64_t val;
        int ret = read(_eventfd, &val, sizeof(val));
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            ERR_LOG("ReadEventFd ERROR!");
            exit(-1);
        }
    }

    bool IsInLoop()
    {
        return std::this_thread::get_id() == _thread_id;
    }

public:
    EventLoop()
        : _isrunning(false), _thread_id(std::this_thread::get_id()), _task_pool(32), _eventfd(CreateEventFd()), _event_channel(_eventfd, this), _timer_wheel(this)
    {
        // 设置eventfd的可读回调
        _event_channel.SetReadCallBack(std::bind(&EventLoop::ReadEventFd, this));
        // 启动可读事件（放到epoll中）
        _event_channel.EnableRead();
    }

    void Start()
    {
        _isrunning = true;
        std::vector<Channel *> active;
        while (_isrunning)
        {
            // 没有事件触发就会阻塞住
            _poller.Poll(&active);
            for (int i = 0; i < active.size(); ++i)
            {
                // Channel* channel = active[i];
                active[i]->HandleEvents();
            }

            RunAllTask();
        }
    }

    // 其他线程往这个线程中压入任务
    void QueueInLoop(const Functor &func)
    {
        // 还是多线程并发访问的问题
        {
            std::lock_guard lock(_mutex);
            _task_pool.push_back(func);
        }
        // 其他线程在这个线程中插入任务了，怎么让这个线程知道？用eventfd进行事件通知
        WakeUpEventFd();
    }

    void RunInLoop(const Functor &func)
    {
        // 首先判断是否是这个线程，是的话就直接执行，不是的话再放到任务池中执行
        if (IsInLoop())
        {
            func();
        }
        QueueInLoop(func);
    }

public:
    void UpdateEvent(Channel *channel)
    {
        _poller.UpdateEvent(channel);
    }

    void RemoveEvent(Channel *channel)
    {
        _poller.RemoveEvent(channel);
    }

    void AddTimerTask(uint64_t timerid, uint32_t timeout, const OnTimerCallBack &on_time_task)
    {
        _timer_wheel.AddTimerTask(timerid, timeout, on_time_task);
    }

    void RefreshTimerTask(uint64_t timerid)
    {
        _timer_wheel.RefreshTimerTask(timerid);
    }

    void CancelTimerTask(uint64_t timerid)
    {
        _timer_wheel.CancelTimerTask(timerid);
    }

    bool HasTimer(uint64_t timerid)
    {
        return _timer_wheel.HasTimer(timerid);
    }

private:
    bool _isrunning;
    Poller _poller;

    std::vector<Functor> _task_pool;
    std::mutex _mutex;

    int _eventfd;
    Channel _event_channel;

    std::thread::id _thread_id;

    TimerWheel _timer_wheel;
};

void Channel::Update()
{
    _loop->UpdateEvent(this);
}

void Channel::Remove()
{
    _loop->RemoveEvent(this);
}

class LoopThread
{
private:
    void ThreadEntry()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock;
            _loop = &loop;
            _cond.notify_all();
        }
        loop.Start();
    }

public:
    LoopThread()
        : _loop(nullptr)
        , _thread(&LoopThread::ThreadEntry, this)
    {
    }

    EventLoop *GetLoop()
    {
        EventLoop* loop;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cond.wait(lock, [&](){ return _loop != nullptr; });
            loop = _loop;
        }
        return loop;
    }

private:
    std::mutex _mtx;
    std::condition_variable _cond;
    EventLoop *_loop;
    std::thread _thread;
};

class LoopThreadPool
{
public:
    LoopThreadPool(EventLoop* baseloop)
        : _baseloop(baseloop)
        , _thread_count(0)
        , _next_idx(0)
    {}

    void SetThreadCount(int thread_count)
    {
        _thread_count = thread_count;
    }

    void Create()
    {
        if (_thread_count == 0) return;
        _threads.resize(_thread_count);
        _loops.resize(_thread_count);
        for (int i = 0; i < _threads.size(); ++i)
        {
            _threads[i] = new LoopThread();
            _loops[i] = _threads[i]->GetLoop();
        }
    }

    EventLoop* NextLoop()
    {
        if (_thread_count == 0)
        {
            return _baseloop;
        }
        _next_idx = (_next_idx + 1) % _thread_count;
        return _loops[_next_idx];
    }

private:
    std::vector<LoopThread*> _threads;
    std::vector<EventLoop*> _loops;
    EventLoop* _baseloop;
    int _thread_count;
    int _next_idx;
};

// 防止业务线程和自己的线程在并发处理_timers 或者 _wheels，因此在自己的loop线程中放到任务池中
void TimerWheel::AddTimerTask(uint64_t timerid, int timeout, const OnTimerCallBack &on_time_task)
{
    _loop->RunInLoop(std::bind(&TimerWheel::AddTimerTaskInloop, this, timerid, timeout, on_time_task));
}

void TimerWheel::RefreshTimerTask(uint64_t timeid)
{
    _loop->RunInLoop(std::bind(&TimerWheel::RefreshTimerTaskInLoop, this, timeid));
}

void TimerWheel::CancelTimerTask(uint64_t timeid)
{
    _loop->RunInLoop(std::bind(&TimerWheel::CancelTimerInLoop, this, timeid));
}

class Any
{
public:
    class Holder
    {
    public:
        virtual ~Holder() = 0;
        virtual const std::type_info &type() = 0;
        virtual Holder *clone() = 0;
    };

    template <class T>
    class PlaceHolder : public Holder
    {
    public:
        PlaceHolder(const T &val)
            : _val(val)
        {
        }

        const std::type_info &type()
        {
            return typeid(T);
        }

        Holder *clone()
        {
            return new PlaceHolder<T>(_val);
        }

    public:
        T _val;
    };

public:
    Any()
        : _holder(nullptr)
    {
    }

    template <class T>
    Any(const T &val)
    {
        _holder = new PlaceHolder<T>(val);
    }

    Any(const Any &other)
    {
        if (other._holder == nullptr)
            _holder = nullptr;
        else
        {
            _holder = other._holder->clone();
        }
    }

    const std::type_info &type()
    {
        return (_holder ? _holder->type() : typeid(void));
    }

    Any &swap(Any &other)
    {
        std::swap(_holder, other._holder);
        return *this;
    }

    template <class T>
    T *GetVal()
    {
        return &((PlaceHolder<T> *)_holder)->_val;
    }

    // 赋值重载
    template <class T>
    Any &operator=(const T &val)
    {
        Any(val).swap(*this);
        return *this;
    }

    Any &operator=(Any other)
    {
        other.swap(*this);
        return *this;
    }

private:
    Holder *_holder;
};

class Connection;
using PtrConnection = std::shared_ptr<Connection>;

// 外部传过来
using ConnectedCallBack = std::function<void(PtrConnection)>;
using MessageCallBack = std::function<void(PtrConnection, Buffer *)>;
using AnyEventCallBack = std::function<void(PtrConnection)>;
using CloseCallBack = std::function<void(PtrConnection)>;
using ErrorCallBack = std::function<void(PtrConnection)>;

typedef enum
{
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED
} STATUS;

class Connection : public std::enable_shared_from_this<Connection>
{
private:
    // Handle Read 只负责读取数据，然后调用回调函数进行处理
    void HandleRead()
    {
        // 用socket接收数据
        char buff[65535] = {0};
        int ret = _socket.RecvNonBlock(buff, sizeof(buff));
        if (ret == -1)
        {
            // 出错了，不能直接关闭链接，要先处理缓冲区中的数据
            ShutDownInLoop();
        }
        // 放到buffer缓冲区中
        _in_buffer.WriteAndPush(buff, ret);
        if (_in_buffer.GetReadableNum() > 0)
        {
            // 防止在message_cb中销毁这个Connection
            if (_message_cb)
                _message_cb(shared_from_this(), &_in_buffer);
        }
    }

    void HandleWrite()
    {
        // 当文件描述符触发写事件的时候，调用这个函数
        // 准确来说是OnMessage处理完成之后放到发送缓冲区
        if (_out_buffer.GetReadableNum() > 0)
        {
            int ret = _socket.SendNonBlock(_out_buffer.ReadPosition(), _out_buffer.GetReadableNum());
            if (ret < 0)
            {
                if (_in_buffer.GetReadableNum() > 0)
                {
                    if (_message_cb)
                    {
                        _message_cb(shared_from_this(), &_in_buffer);
                    }
                }
                return Release(); // 真正的关闭链接
            }
            _out_buffer.MoveReadOffset(_out_buffer.GetReadableNum());
            if (_out_buffer.GetReadableNum() == 0)
            {
                _channel.DisableWrite();
                if (_status == DISCONNECTING)
                {
                    _status = DISCONNECTED;
                    return Release();
                }
            }
        }
    }

    void HandleClose()
    {
        if (_in_buffer.GetReadableNum() > 0)
        {
            if (_message_cb)
            {
                _message_cb(shared_from_this(), &_in_buffer);
            }
        }
        _channel.DisableAll();
        Release();
    }

    void HandleError()
    {
        return HandleClose();
    }

    void HandleAnyEvent()
    {
        if (_enable_inactive_release)
            _loop->RefreshTimerTask(_timer_id);
        if (_event_cb)
            _event_cb(shared_from_this());
    }

    void EstablishedInLoop()
    {
        assert(_status == CONNECTING);
        _status = CONNECTED;
        _channel.EnableRead();
        if (_conn_cb)
            _conn_cb(shared_from_this());
    }

    void ReleaseInLoop()
    {
        _status = DISCONNECTED;
        _channel.Remove();
        _socket.Close();

        if (_loop->HasTimer(_timer_id))
        {
            _loop->CancelTimerTask(_timer_id);
        }

        if (_close_cb)
            _close_cb(shared_from_this());
        if (_server_close_cb)
            _server_close_cb(shared_from_this());
    }

    void SendInLoop(Buffer &buf)
    {
        if (_status == DISCONNECTED)
            return;
        _out_buffer.WriteBufferAndPush(buf);
        if (!_channel.WriteAble())
            _channel.EnableWrite();
    }

    void ShutDownInLoop()
    {
        _status = DISCONNECTING;
        if (_in_buffer.GetReadableNum() > 0)
        {
            if (_message_cb)
                _message_cb(shared_from_this(), &_in_buffer);
        }
        if (_out_buffer.GetReadableNum() > 0)
        {
            if (!_channel.WriteAble())
                _channel.EnableWrite();
        }
        if (_out_buffer.GetReadableNum() == 0)
            Release();
    }

    void EnableInactiveReleaseInLoop(int sec)
    {
        _enable_inactive_release = true;
        if (_loop->HasTimer(_timer_id))
            _loop->RefreshTimerTask(_timer_id);
        else
            _loop->AddTimerTask(_timer_id, sec, std::bind(&Connection::Release, this));
    }

    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;
        if (_loop->HasTimer(_timer_id))
            _loop->CancelTimerTask(_timer_id);
    }

    void UpgradeInLoop(const Any &context, const ConnectedCallBack &conn_cb,
                       const MessageCallBack &message_cb, const AnyEventCallBack &event_cb,
                       const CloseCallBack &close_cb, const ErrorCallBack &error_cb, const CloseCallBack &server_close_cb)
    {
        _context = context;
        _conn_cb = conn_cb;
        _message_cb = message_cb;
        _event_cb = event_cb;
        _close_cb = close_cb;
        _error_cb = error_cb;
        _server_close_cb = server_close_cb;
    }

public:
    Connection(EventLoop *loop, int sockfd, uint64_t timerid, uint64_t connid)
        : _loop(loop), _status(CONNECTING), _sockfd(sockfd), _channel(_sockfd, _loop), _socket(_sockfd), _timer_id(timerid), _conn_id(connid), _enable_inactive_release(false)
    {
        _channel.SetReadCallBack(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseCallBack(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallBack(std::bind(&Connection::HandleError, this));
        _channel.SetAnyCallBack(std::bind(&Connection::HandleAnyEvent, this));
    }

    // 外部设置五种回调函数
    void SetConnectedCallBack(const ConnectedCallBack &cb)
    {
        _conn_cb = cb;
    }

    void SetMessageCallBack(const MessageCallBack &cb)
    {
        _message_cb = cb;
    }

    void SetAnyEventCallBack(const AnyEventCallBack &cb)
    {
        _event_cb = cb;
    }

    void SetCloseCallBack(const CloseCallBack &cb)
    {
        _close_cb = cb;
    }

    void SetErrorCallBack(const ErrorCallBack &cb)
    {
        _error_cb = cb;
    }

    void SetServerCloseCallBack(const CloseCallBack &cb)
    {
        _server_close_cb = cb;
    }

    ~Connection()
    {
        DBG_LOG("Connection Released:%p", this);
    }

    int Fd()
    {
        return _sockfd;
    }

    int ConnId()
    {
        return _conn_id;
    }

    int TimerId()
    {
        return _timer_id;
    }

    void SetContext(const Any &context)
    {
        _context = context;
    }

    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
    }

    void Send(const char *data, size_t len)
    {
        Buffer buf;
        buf.WriteAndPush(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, buf));
    }

    void ShutDown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutDownInLoop, this));
    }

    void Release()
    {
        _loop->RunInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }

    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }

    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }

    void Upgrade(const Any &context, const ConnectedCallBack &conn_cb,
                 const MessageCallBack &message_cb, const AnyEventCallBack &event_cb,
                 const CloseCallBack &close_cb, const ErrorCallBack &error_cb, const CloseCallBack &server_close_cb)
    {
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn_cb, message_cb, event_cb, close_cb, error_cb, server_close_cb));
    }

private:
    EventLoop *_loop;
    STATUS _status;

    ConnectedCallBack _conn_cb;
    MessageCallBack _message_cb;
    AnyEventCallBack _event_cb;
    CloseCallBack _close_cb;
    CloseCallBack _server_close_cb;
    ErrorCallBack _error_cb;

    // 每个链接都要有自己的缓冲区
    Buffer _in_buffer;
    Buffer _out_buffer;

    int _sockfd;
    Channel _channel;
    Socket _socket;

    bool _enable_inactive_release;

    uint64_t _conn_id;
    uint64_t _timer_id;

    Any _context;
};

using AcceptCallBack = std::function<void()>;
class Acceptor
{
private:
    int ServerInit()
    {
        int ret = _socket.CreateServer(_port);
        return ret;
        if (_accept_cb)
            _accept_cb();
    }

    void HandlerNewConnection()
    {
        int io_fd = _socket.Accept();
        if (io_fd < 0)
            return ;
    }

public:
    Acceptor(uint16_t port, EventLoop* base_loop)
        : _port(port)
        , _fd(ServerInit())
        , _channel(_fd, _base_loop)
        , _base_loop(base_loop)
    {
        _channel.SetReadCallBack(std::bind(&Acceptor::HandlerNewConnection, this));
    }

    void Established()
    {
        _channel.EnableRead();
    }

    void SetAcceptCallBack(const AcceptCallBack& accept_cb)
    {
        _accept_cb = accept_cb;
    }

    int Fd()
    {
        return _fd;
    }

private:
    uint16_t _port;
    Socket _socket;
    int _fd;
    Channel _channel;
    EventLoop* _base_loop;
    AcceptCallBack _accept_cb;
};


class TcpServer
{
private:
    void OnAccept()
    {
        EventLoop* nextloop = _pool.NextLoop();
        Channel io_channel(_accept.Fd(), nextloop);
        PtrConnection newconn = std::make_shared<Connection>(nextloop, _accept.Fd(), _timer_id, _conn_id);
        
        newconn->SetConnectedCallBack(_conn_cb);
        newconn->SetMessageCallBack(_message_cb);
        newconn->SetAnyEventCallBack(_any_cb);
        newconn->SetCloseCallBack(_close_cb);
        newconn->SetErrorCallBack(_error_cb);
        //newconn->SetServerCloseCallBack(std::bind(&TcpServer::ServerClose, this, std::placeholders::_1));
        newconn->EnableInactiveRelease(_timeout);
        newconn->Established();

        _conn_map.insert(std::make_pair(_conn_id, newconn));
        _conn_id++;
        _timer_id++;
    }

    void ServerClose(int conn_id)
    {
        auto it = _conn_map.find(conn_id);
        if (it != _conn_map.end())
        {
            _conn_map.erase(it);
        }
        return;
    }
public:
    TcpServer(int port)
        : _port(port)
        , _thread_count(0)
        , _enable_inactive_release(true)
        , _conn_id(0)
        , _timer_id(0)
        , _accept(port, &_base_loop)
        , _pool(&_base_loop)
    {
        _accept.SetAcceptCallBack(std::bind(&TcpServer::OnAccept, this));
        _accept.Established();
    }

    void SetConnectionCallBack(const ConnectedCallBack& cb)
    {
        _conn_cb = cb;
    }

    void SetMessageCallBack(const MessageCallBack& cb)
    {
        _message_cb = cb;
    }

    void SetErrorCallBack(const ErrorCallBack& cb)
    {
        _error_cb = cb;
    }

    void SetCloseCallBack(const CloseCallBack& cb)
    {
        _close_cb = cb;
    }

    void SetEventCallBack(const AnyEventCallBack& cb)
    {
        _any_cb = cb;
    }

    void SetServerCloseCallBack(const CloseCallBack& cb)
    {
        _server_close_cb = cb;
    }

    void SetThreadCountAndCreate(int thread_count)
    {
        _thread_count = thread_count;
        _pool.Create();
    }
    
    void Start()
    {
        //_accept.Established();
        _base_loop.Start();
    }

private:
    bool _enable_inactive_release;
    int _timeout;
    int _port;
    int _thread_count;
    EventLoop _base_loop;
    LoopThreadPool _pool;
    int _conn_id;
    int _timer_id;
    std::unordered_map<uint64_t, PtrConnection> _conn_map;
    Acceptor _accept;

    ConnectedCallBack _conn_cb;
    MessageCallBack _message_cb;
    ErrorCallBack _error_cb;
    CloseCallBack _close_cb;
    AnyEventCallBack _any_cb;
    CloseCallBack _server_close_cb;
};