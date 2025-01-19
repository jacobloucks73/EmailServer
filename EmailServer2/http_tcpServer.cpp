#include "http_tcpServer.h"

namespace http
{
    TcpServer::TcpServer(const std::string& ipAddress, int port)
        : m_socket(INVALID_SOCKET),
        m_new_socket(INVALID_SOCKET),
        m_ip_address(ipAddress),
        m_port(port)
    {
        // Zero out the socket address structure
        ZeroMemory(&m_socketAddress, sizeof(m_socketAddress));
        m_socketAddress_len = sizeof(m_socketAddress);

        // Prepare a very basic HTTP/1.1 200 OK response
        m_serverMessage =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<html><head><title>Test HTTP Server</title></head>"
            "<body><h1>Welcome to my test HTTP server!</h1></body></html>\r\n";

        // Attempt to initialize the server
        if (startServer() != 0)
        {
            exitWithError("Failed to start server");
        }
    }

    TcpServer::~TcpServer()
    {
        closeServer();
    }

    int TcpServer::startServer()
    {
        // Initialize Winsock
        int wsaResult = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
        if (wsaResult != 0)
        {
            exitWithError("WSAStartup failed");
            return 1;
        }

        // Create the socket
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET)
        {
            exitWithError("Cannot create socket");
            return 1;
        }

        // Configure the sockaddr_in structure
        m_socketAddress.sin_family = AF_INET;
        m_socketAddress.sin_port = htons(m_port);
        m_socketAddress.sin_addr.s_addr = inet_addr(m_ip_address.c_str());

        // Optionally set the SO_REUSEADDR socket option
        BOOL optval = TRUE;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
            (const char*)&optval, sizeof(optval)) == SOCKET_ERROR)
        {
            exitWithError("Cannot set SO_REUSEADDR");
            return 1;
        }

        // Bind the socket
        if (bind(m_socket,
            reinterpret_cast<struct sockaddr*>(&m_socketAddress),
            m_socketAddress_len) == SOCKET_ERROR)
        {
            exitWithError("Cannot bind socket to address");
            return 1;
        }

        return 0; // success
    }

    void TcpServer::startListen()
    {
        // Start listening (backlog of 20)
        if (listen(m_socket, 20) == SOCKET_ERROR)
        {
            exitWithError("Socket listen failed");
        }

        std::ostringstream ss;
        ss << "\n*** Listening on ADDRESS: " << m_ip_address
            << " PORT: " << m_port << " ***\n" << std::endl;
        log(ss.str());

        // Accept connections in a loop
        while (true)
        {
            acceptConnection(m_new_socket);
            log("------ Client connected ------");

            // Receive the request
            const int BUFFER_SIZE = 30720;
            char buffer[BUFFER_SIZE] = { 0 };
            int bytesReceived = recv(m_new_socket, buffer, BUFFER_SIZE, 0);

            if (bytesReceived == SOCKET_ERROR)
            {
                exitWithError("Failed to receive bytes from client socket connection");
            }
            else if (bytesReceived == 0)
            {
                log("Client disconnected immediately or no data received.");
            }
            else
            {
                // Print out the request
                std::string request(buffer, bytesReceived);
                log("------ Received Request from client ------\n" + request);

                // Send a response
                int totalBytesSent = 0;
                int toSend = static_cast<int>(m_serverMessage.size());

                while (totalBytesSent < toSend)
                {
                    int bytesSent = send(m_new_socket,
                        m_serverMessage.c_str() + totalBytesSent,
                        toSend - totalBytesSent,
                        0);
                    if (bytesSent == SOCKET_ERROR)
                    {
                        log("Error sending response to the client.");
                        break;
                    }
                    totalBytesSent += bytesSent;
                }

                if (totalBytesSent == toSend)
                {
                    log("------ Response sent to client ------\n");
                }
            }

            // Close this connection
            closesocket(m_new_socket);
            m_new_socket = INVALID_SOCKET;
        }
    }

    void TcpServer::acceptConnection(SOCKET& new_socket)
    {
        new_socket = accept(m_socket,
            reinterpret_cast<struct sockaddr*>(&m_socketAddress),
            &m_socketAddress_len);

        if (new_socket == INVALID_SOCKET)
        {
            std::ostringstream ss;
            ss << "Server failed to accept incoming connection. Error: " << WSAGetLastError();
            exitWithError(ss.str());
        }
    }

    void TcpServer::closeServer()
    {
        // Close the main listening socket if not already closed
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        // Close the connection socket if open
        if (m_new_socket != INVALID_SOCKET)
        {
            closesocket(m_new_socket);
            m_new_socket = INVALID_SOCKET;
        }

        // Cleanup Winsock
        WSACleanup();
    }

    void TcpServer::log(const std::string& message)
    {
        std::cout << message << std::endl;
    }

    void TcpServer::exitWithError(const std::string& errorMessage)
    {
        std::cerr << "WSA Error Code: " << WSAGetLastError() << std::endl;
        std::cerr << "ERROR: " << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
