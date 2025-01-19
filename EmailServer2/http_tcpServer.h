#ifndef INCLUDED_HTTP_TCPSERVER_WINDOWS
#define INCLUDED_HTTP_TCPSERVER_WINDOWS

#include <string>
#include <iostream>
#include <sstream>
#include <cstdlib>   // for exit
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with ws2_32.lib (for MSVC). 
// If using MSVC, you can optionally use:
// #pragma comment (lib, "Ws2_32.lib")

namespace http
{
    class TcpServer
    {
    public:
        // Constructor with default IP "0.0.0.0" and port 8080
        TcpServer(const std::string& ipAddress = "0.0.0.0", int port = 8080);
        ~TcpServer();

        // Start listening for connections (blocking call)
        void startListen();

    private:
        // Windows Sockets
        SOCKET m_socket;       // "Listening" socket
        SOCKET m_new_socket;   // A "per-connection" socket

        // WSAData
        WSAData m_wsaData;

        // Server address
        struct sockaddr_in m_socketAddress;
        int m_socketAddress_len;

        // IP address and port
        std::string m_ip_address;
        int m_port;

        // A default server message to send back
        std::string m_serverMessage;

        // Initialize Winsock and create the server socket (called in constructor)
        int startServer();

        // Clean up resources (called in destructor)
        void closeServer();

        // Helper methods for logging and error-handling
        void log(const std::string& message);
        void exitWithError(const std::string& errorMessage);

        // Accept a new client connection
        void acceptConnection(SOCKET& new_socket);
    };
}

#endif
