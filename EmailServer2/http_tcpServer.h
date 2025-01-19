#ifndef INCLUDED_HTTP_TCPSERVER
#define INCLUDED_HTTP_TCPSERVER

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <winsock.h>

namespace http
{
    class TcpServer
    {
    public:
        TcpServer(std::string ip_address, int port);
        ~TcpServer();
        
    private:
        int startServer();
        void closeServer();

        int m_port;
        std::string m_ip_address;
        SOCKET m_socket;
        SOCKET m_new_socket;
        long m_incomingMessage;
        struct sockaddr_in m_socketAddress;
        int m_socketAddress_len;
        std::string m_serverMessage;
        WSADATA m_wsaData;
    };
} // namespace http
#endif