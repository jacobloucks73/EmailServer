#include "http_tcpServer.h"

#include <iostream>
#include <sstream>
#include <unistd.h>

namespace http
{
    TcpServer::TcpServer(std::string ip_address, int port)
     :m_ip_address(ip_address),
        m_port(port),
        m_socket(),
        m_new_socket(),
        m_incomingMessage(),
        m_socketAddress(),
        m_socketAddress_len(sizeof(m_socketAddress)),
        m_serverMessage(buildResponse())
    {
        startServer();
    }

    TcpServer::~TcpServer()
    {
        closeServer();
    }

    int TcpServer::startServer()
    {
        if (WSAStartup(MAKEWORD(2, 0), &m_wsaData) != 0)
        {
            exitWithError("WSAStartup failed");
        }
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0)
        {
            exitWithError("Cannot create socket");
            return 1;
        }
        return 0;
    }

    void TcpServer::closeServer()
    {
        closesocket(m_socket);
        closesocket(m_new_socket);
        WSACleanup();
        exit(0);
    }

    void log(const std::string& message)
    {
        std::cout << message << std::endl;
    }

    void exitWithError(const std::string& errorMessage)
    {
        log("ERROR: " + errorMessage);
        exit(1);
    }
} // namespace http
namespace
{
    void log(const std::string  &message) 
    {
        std::cout << message << "/n";
    }

    void exitWithError(const std::string &errorMessage)
    {
        std::cout << WSAGetLastError() << "/n";
        log("ERROR: " + errorMessage);
        exit(1);
    }
}