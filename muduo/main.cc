#include "server.hpp"

int main()
{
    TcpServer server(8888);
    server.Start();
    return 0;
}