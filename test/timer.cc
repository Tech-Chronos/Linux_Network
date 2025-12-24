#include <iostream>
#include <unistd.h>
#include <sys/timerfd.h>
#include <stdint.h>

int main()
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0)
    {
        std::cerr << "tfd error!" << std::endl;
    }

    struct itimerspec its = {0};
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;

    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;

    timerfd_settime(tfd, 0, &its, nullptr);

    while (true)
    {
        uint64_t expire = 0;
        int ret = read(tfd, &expire, sizeof(expire));
        std::cout << "已经过期了 " << expire << " 次" << std::endl;;
    }

    return 0;
}