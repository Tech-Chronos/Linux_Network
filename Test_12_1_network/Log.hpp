#pragma once

#include <iostream>
#include <string>

#include <ctime>
#include <cstdarg>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "LockGuard.hpp"

#define SIZE 1024

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


enum level
{
    DEBUG = 1,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// 日志信息
struct LogMessage
{
    std::string _level; // 日志等级
    pid_t _pid; // 进程pid
    std::string _file_name; // 日志打印的文件名
    int _line_num;  // 日志输出的行号
    std::string _cur_time;  // 当前打印日志的时间
    std::string _mes_info;  // 真正的信息
};

std::string SplitTime(struct tm* now)
{
    // std::string year = std::to_string(now->tm_year + 1900);
    // std::string month = std::to_string(now->tm_mon + 1);
    // std::string day = std::to_string(now->tm_mday);

    // std::string hour = std::to_string(now->tm_hour);
    // std::string minute = std::to_string(now->tm_min);
    // std::string second = std::to_string(now->tm_sec);

    int year = now->tm_year + 1900;
    int month = now->tm_mon + 1;
    int day = now->tm_mday;

    int hour = now->tm_hour;
    int minute = now->tm_min;
    int second = now->tm_sec;

    char buffer[SIZE];
    snprintf(buffer, SIZE, "%d-%02d-%02d %d:%02d:%02d", year, month, day, hour, minute, second);
    //log._cur_time = year + "-" + month + "-" + day + " " + hour + ":" + minute + ":" + second;

    return buffer;
}

std::string LogLevelToString(int level)
{
    switch (level)
    {
    case 1:
        return "DEBUG";
    case 2:
        return "INFO";
    case 3:
        return "WARNING";
    case 4:
        return "ERROR";
    case 5:
        return "FATAL";
    default:
        break;
    }
    return "";
}

std::string GetMessageInfo(const char* format, va_list ap)
{
    char buffer[SIZE];
    vsnprintf(buffer, 1024, format, ap);
    return buffer;
}

void PrintLog(LogMessage& log, std::string print_loc, std::string file_name)
{
    LockGuard lock(&mutex);
    if (print_loc == "SCREEN")
        printf("[%s][%d][%s][%d][%s] %s\n", log._level.c_str(), log._pid, log._file_name.c_str(), log._line_num, log._cur_time.c_str(), log._mes_info.c_str());
    else 
    {
        int fd = open(file_name.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
        char buffer[SIZE];
        memset(buffer, 0, SIZE);
        snprintf(buffer, SIZE, "[%s][%d][%s][%d][%s] %s\n", log._level.c_str(), log._pid, log._file_name.c_str(), log._line_num, log._cur_time.c_str(), log._mes_info.c_str());
        write(fd, buffer, strlen(buffer));
        close(fd);
    }
}

class Log
{
public:
    Log(std::string print_loc = "SCREEN", std::string file_name = "log.txt")
        :_print_loc(print_loc)
        ,_file_name(file_name)
    {}

    std::string GetPrintLoc()
    {
        return _print_loc;
    }

    void LoggingMessage(int level, const char* filename, int line_num, const char* format , ...)
    {
        // 1. 获取pid
        LogMessage log;
        log._pid = getpid();

        // 2. 获取时间戳
        time_t time_stamp = time(nullptr);
        struct tm* now = localtime(&time_stamp);
        
        log._cur_time = SplitTime(now);

        // 3. 获取日志等级
        log._level = LogLevelToString(level);

        // 4. 获取 message info
        va_list ap;
        va_start(ap, format);
        log._mes_info = GetMessageInfo(format, ap);
        va_end(ap);

        // 5. 打印日志的文件名称
        log._file_name = filename;

        // 6. 打印日志的行号
        log._line_num = line_num;

        // 7. 打印日志
        PrintLog(log, _print_loc, _file_name);
    }

private:
    std::string _print_loc;
    std::string _file_name;
};

#define LOG(Level, Format, ...)                                        \
    do                                                                 \
    {                                                                  \
        lg.LoggingMessage(Level, __FILE__, __LINE__,Format, ##__VA_ARGS__); \
    } while (0)


