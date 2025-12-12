#pragma once
#include <iostream>
#include <queue>

template<class F>
class Thread
{
private:
    void Excute()
    {
        _is_running = true;
        _func(_name);
        _is_running = false;
    }

public:
    Thread(std::string name, F func)
        :_name(name)
        ,_is_running(false)
        ,_func(func)
    {}

    Thread() = default;

    static void* HandlerTask(void* args)
    {
        Thread<F>* self = static_cast<Thread<F>*>(args);
        self->Excute();

        return nullptr;
    }

    void Start()
    {
        _is_running = true;
        pthread_create(&_tid, nullptr, HandlerTask, this);
    }

    void Cancel()
    {
        if (_is_running)
            pthread_cancel(_tid);
    }

    void Join()
    {
        pthread_join(_tid, nullptr);
    }

private:
    pthread_t _tid;
    std::string _name;

    bool _is_running;

    F _func;
};

