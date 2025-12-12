#include "UdpServer.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        LOG(FATAL, "use method: %s Port", argv[0]);
        exit(-1);
    }

    std::unique_ptr<UdpServer> usptr = std::make_unique<UdpServer>(std::stoi(argv[1]));

    usptr->Init();

    usptr->Start();
    return 0;
}