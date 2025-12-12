#include "UdpServer.hpp"
#include "Dict.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        LOG(FATAL,"USAGE -> %s PORT", argv[0]);
        exit(-1);
    }

    Dict dict;
    func_t translate = std::bind(&Dict::translate, dict, std::placeholders::_1);
    std::unique_ptr<UdpServer> usptr = std::make_unique<UdpServer>(std::stoi(argv[1]), translate);

    usptr->Init();
    usptr->Start();
    return 0;
}