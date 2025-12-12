#pragma once
#include "Thread.hpp"
#include "LockGuard.hpp"

using task_t = std::function<void(std::string&)>;

template <class F>
class ThreadPool
{
private:
    bool IsEmpty()
    {
        return _tasks_queue.empty();
    }
    void Lock()
    {
        pthread_mutex_lock(&_mutex);
    }

    void Unlock()
    {
        pthread_mutex_unlock(&_mutex);
    }

    bool Wait()
    {
        return pthread_cond_wait(&_cond, &_mutex);
    }

    void WakeUp()
    {
        pthread_cond_signal(&_cond);
    }

    void WakeUpAll()
    {
        pthread_cond_broadcast(&_cond);
    }

    void HandlerTask(std::string &name)
    {
        while (true)
        {
            Lock();
            // 但凡队伍里面有任务，就不能退出，必须执行完所有的任务才能退出
            // while 防止假唤醒
            while (IsEmpty() && _is_running)
            {
                ++_sleep_threads_num;
                Wait();
                --_sleep_threads_num;
            }

            // 退出条件
            if (IsEmpty() && !_is_running)
            {
                Unlock();
                break;
            }

            // 有任务
            F task_thread = _tasks_queue.front();
            _tasks_queue.pop();
            Unlock();
            task_thread(name);
        }
    }

    ThreadPool(int thread_num = 5)
        : _thread_num(thread_num), _sleep_threads_num(0), _is_running(false)
    {
        pthread_mutex_init(&_mutex, nullptr);
        pthread_cond_init(&_cond, nullptr);
    }

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    void Init()
    {
        _is_running = true;

        for (int i = 0; i < _thread_num; ++i)
        {
            std::string name = "thread [" + std::to_string(i + 1) + "]";
            task_t task = std::bind(&ThreadPool::HandlerTask, this, std::placeholders::_1);
            _threads.emplace_back(name, task);
        }
    }

    void Start()
    {
        _is_running = true;
        for (auto &thread : _threads)
        {
            thread.Start();
        }
    }

public:
    static ThreadPool *GetSingleInstance(int thread_num = 5)
    {
        // 如果已经创建了，可能多线程都卡在这里，用双层循环判断
        if (_tp_ptr == nullptr)
        {
            // 如果多线程同时调用这个单例，会出现问题
            LockGuard lock(&_single_mutex);
            if (_tp_ptr == nullptr)
            {
                // 可能很多线程都在这里都被切走了，都没有建立对象，因此要加锁保护
                _tp_ptr = new ThreadPool<F>(thread_num);
                _tp_ptr->Init();

                _tp_ptr->Start();
            }
        }
        return _tp_ptr;
    }

    void Enqueue(const F func)
    {
        Lock();

        // 防止线程退出之后继续在任务队列中插入任务
        if (_is_running)
        {
            _tasks_queue.push(func);
            // 这边插入任务之后要通知线程有任务了
            if (_sleep_threads_num > 0)
                WakeUp();
        }

        Unlock();
    }

    void Stop()
    {
        Lock();
        // 如果有线程在休眠我要唤醒他们
        _is_running = false;
        WakeUpAll();

        Unlock();

        // 别忘了join！不然主线程析构了，锁和环境变量都destroy了
        // 但是还有线程在执行任务，就core dumped了
        for (auto &thread : _threads)
        {
            thread.Join();
        }
    }

    ~ThreadPool()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond);
    }

private:
    int _thread_num;
    int _sleep_threads_num;

    bool _is_running;
    std::vector<Thread<F>> _threads; // 管理线程
    std::queue<F> _tasks_queue;      // 管理任务队列

    pthread_mutex_t _mutex; // 对队列资源的保护
    pthread_cond_t _cond;   // 条件变量看是否有任务

    static ThreadPool<F> *_tp_ptr;
    static pthread_mutex_t _single_mutex;
};

template <class F>
ThreadPool<F> *ThreadPool<F>::_tp_ptr = nullptr;

template <class F>
pthread_mutex_t ThreadPool<F>::_single_mutex = PTHREAD_MUTEX_INITIALIZER;
