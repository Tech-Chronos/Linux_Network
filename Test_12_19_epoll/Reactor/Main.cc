#include "Reactor.hpp"
#include "Listener.hpp"


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        LOG(ERROR, "USAGE METHOD -> %s PORT", argv[0]);
        exit(-1);
    }

    uint16_t port = std::stoi(argv[1]);
    std::unique_ptr<Listener> listener = std::make_unique<Listener>(port);

    InAddr addr("0.0.0.0", port);

    int listen_sockfd = listener->Init();
    
    std::unique_ptr<Reactor> R = std::make_unique<Reactor>();
    R->SetOnConnect(std::bind(&Listener::Accept, Listener(port), std::placeholders::_1));
    
    R->SetOnNormalHandler(  std::bind(&HandlerIO::RecvMessage, HandlerIO(),std::placeholders::_1),
                            std::bind(&HandlerIO::SendMessage, HandlerIO(),std::placeholders::_1),
                            std::bind(&HandlerIO::HandleExcept, HandlerIO(),std::placeholders::_1)
                         );

    R->AddConnection(listen_sockfd, EPOLLIN | EPOLLET, ListenConnection, addr);

    R->Dispatch();
    
    return 0;
}