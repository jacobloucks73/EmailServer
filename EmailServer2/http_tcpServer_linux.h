#ifndef INCLUDED_HTTP_TCPSERVER_LINUX
#define INCLUDED_HTTP_TCPSERVER_LINUX

#include <string>
#include <iostream>
#include <sstream>
#include <cstdlib>      // for exit
#include <unistd.h>     // for close(), read(), write()
#include <arpa/inet.h>  // for AF_INET, SOCK_STREAM, etc.
#include <sys/socket.h> // for socket(), bind(), listen(), accept()
#include <netinet/in.h> // for sockaddr_in
#include <cstring>      // for memset()

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
        // Sockets
        int m_socket;       // The "listening" socket
        int m_new_socket;   // A "per-connection" socket

        // Server address
        struct sockaddr_in m_socketAddress;
        socklen_t m_socketAddress_len;

        // IP address and port
        std::string m_ip_address;
        int m_port;

        // A default server message to send back
        std::string m_serverMessage;

        // Initialize the server (called in constructor)
        int startServer();

        // Clean up resources (called in destructor)
        void closeServer();

        // Helper methods for logging and error-handling
        void log(const std::string& message);
        void exitWithError(const std::string& errorMessage);

        // Accept a new client connection
        void acceptConnection(int& new_socket);
    };
}

#endif
