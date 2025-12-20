#ifndef __LOCK_GUARD__
#define __LOCK_GUARD__
#include <iostream>
#include <string>
#include <vector>
#include <functional>

#include <unistd.h>
#include <pthread.h>

class LockGuard
{
public:
    LockGuard(pthread_mutex_t* mutex)
        :_mutex(mutex)
    {
        pthread_mutex_lock(_mutex);
    }

    ~LockGuard()
    {
        pthread_mutex_unlock(_mutex);
    }
private:
    pthread_mutex_t* _mutex;
};


#endif

