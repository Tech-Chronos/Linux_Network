#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stdint.h>
#include <algorithm>

#include <unistd.h>

using task_t = std::function<void()>;
using release_t = std::function<void()>;

class TimerTask
{
public:
    TimerTask(uint64_t id, uint32_t timeout, task_t task)
        : _id(id), _timeout(timeout), _task_cb(task), _cancel(false)
    {
    }

    void SetRelease(release_t release)
    {
        _release = release;
    }

    uint32_t GetTimeOut()
    {
        return _timeout;
    }

    void Cancel()
    {
        _cancel = false;
    }

    ~TimerTask()
    {
        std::cout << "Timer task delete" << std::endl;
        if (_cancel)
            _task_cb();
        _release();
    }

private:
    uint64_t _id;
    uint32_t _timeout;
    task_t _task_cb;
    release_t _release;
    bool _cancel;
};

class TimerWheel
{
private:
    void RemoveTimer(uint64_t id)
    {
        std::unordered_map<uint64_t, std::weak_ptr<TimerTask>>::iterator it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }

public:
    TimerWheel(int capacity)
        : _capacity(capacity), _tick(0)
    {
        // resize会调用对应位置的构造函数
        _wheel.resize(capacity);
    }

    void AddTimer(uint64_t id, uint32_t timeout, task_t task)
    {
        std::shared_ptr<TimerTask> tsp = std::make_shared<TimerTask>(id, timeout, task);

        tsp->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));

        size_t pos = (_tick + timeout) % _capacity;
        _wheel[pos].push_back(tsp);

        _timers[id] = std::weak_ptr<TimerTask>(tsp);
    }

    void RefreshTimer(uint64_t id)
    {
        std::unordered_map<uint64_t, std::weak_ptr<TimerTask>>::iterator it = _timers.find(id);
        if (it != _timers.end())
        {
            std::shared_ptr<TimerTask> tsp(_timers[id].lock());
            if (!tsp) return;
            size_t pos = (_tick + it->second.lock()->GetTimeOut()) % _capacity;
            _wheel[pos].push_back(tsp);
        }
        return;
    }

    void SetTimerCancel(uint64_t id)
    {
        std::unordered_map<uint64_t, std::weak_ptr<TimerTask>>::iterator it = _timers.find(id);
        if (it == _timers.end()) return;
        
        std::shared_ptr<TimerTask> tsp(_timers[id].lock());
        if (!tsp) return;
        tsp->Cancel();
        std::cout << "id :" << id << " 被取消执行了！" << std::endl;
    }

    void RunTimer()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear();
    }

    ~TimerWheel()
    {
    }

private:
    int _capacity;
    int _tick;
    std::vector<std::vector<std::shared_ptr<TimerTask>>> _wheel;
    std::shared_ptr<TimerTask> _tsp;
    std::unordered_map<uint64_t, std::weak_ptr<TimerTask>> _timers;
};

class Test
{
public:
    Test()
    {
        std::cout << "构造" << std::endl;
    }

    ~Test()
    {
        std::cout << "析构" << std::endl;
    }

};

void Del(Test* t)
{
    delete t;
}


int main()
{
    TimerWheel tw(60);

    Test* t = new Test();

    tw.AddTimer(888, 5, std::bind(Del, t));

    for (int i = 0; i < 5; ++i)
    {
        sleep(1);
        tw.RefreshTimer(888);
        std::cout << "时间轮更新，5s钟后执行！" << std::endl;
        tw.RunTimer();
    }

    tw.SetTimerCancel(888);

    while (true)
    {
        sleep(1);
        std::cout << "-------------------------" << std::endl;
        tw.RunTimer();
    }

    return 0;
}