#include "http_tcpServer.h"
int main()
{
    using namespace http;
    TcpServer server = TcpServer("0.0.0.0",8080);

    return 0;
    
}