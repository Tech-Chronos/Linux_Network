#pragma once
#include "Log.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

const char *sep = ": ";

class Dict
{
private:
    void LoadDict()
    {
        std::ifstream in("dict.txt");
        if (!in.is_open())
        {
            LOG(FATAL, "open error");
            exit(-1);
        }

        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty())
                continue;
            std::string key = line.substr(0, line.find(sep));
            if (key.empty())
                continue;
            std::string val = line.substr(line.find(sep) + 2);
            if (val.empty())
                continue;
            _dict.insert({key, val});
        }
        in.close();
    }

public:
    Dict()
    {
        LoadDict();
    }

    std::string translate(std::string key)
    {
        std::unordered_map<std::string, std::string>::iterator it = _dict.begin();
        while (it != _dict.end())
        {
            if (it->first == key)
            {
                return it->second;
            }
            ++it;
        }
        return "None";
    }

    ~Dict()
    {
    }

private:
    std::unordered_map<std::string, std::string> _dict;
};