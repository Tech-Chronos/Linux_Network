#include "UdpServer.hpp"
#include "User.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        LOG(FATAL,"USAGE -> %s PORT", argv[0]);
        exit(-1);
    }

    Group gp;
    func_t forward = std::bind(&Group::SendMessageToAll, &gp, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    
    std::unique_ptr<UdpServer> usptr = std::make_unique<UdpServer>(std::stoi(argv[1]), forward);

    usptr->Init();
    usptr->Start();
    return 0;
}