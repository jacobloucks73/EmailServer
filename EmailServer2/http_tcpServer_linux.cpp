/*#include "http_tcpServer_linux.h"

namespace http
{
    TcpServer::TcpServer(const std::string& ipAddress, int port)
        : m_socket(-1),
        m_new_socket(-1),
        m_ip_address(ipAddress),
        m_port(port)
    {
        // Zero out the socket address structure
        std::memset(&m_socketAddress, 0, sizeof(m_socketAddress));
        m_socketAddress_len = sizeof(m_socketAddress);

        // Prepare a very basic HTTP/1.1 200 response
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
        // Create the socket
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0)
        {
            exitWithError("Cannot create socket");
            return 1;
        }

        // Configure the sockaddr_in structure
        m_socketAddress.sin_family = AF_INET;
        m_socketAddress.sin_port = htons(m_port);
        m_socketAddress.sin_addr.s_addr = inet_addr(m_ip_address.c_str());

        // Optionally enable the SO_REUSEADDR socket option
        int opt = 1;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            exitWithError("Cannot set SO_REUSEADDR");
            return 1;
        }

        // Bind socket to the IP and port
        if (bind(m_socket,
            reinterpret_cast<struct sockaddr*>(&m_socketAddress),
            m_socketAddress_len) < 0)
        {
            exitWithError("Cannot bind socket to the given address");
            return 1;
        }

        return 0; // success
    }

    void TcpServer::startListen()
    {
        // Start listening with a backlog of 20
        if (listen(m_socket, 20) < 0)
        {
            exitWithError("Failed to put socket in listening state");
        }

        std::ostringstream ss;
        ss << "\n*** Listening on ADDRESS: " << m_ip_address
            << " PORT: " << m_port << " ***\n" << std::endl;
        log(ss.str());

        // Keep accepting new connections in a loop
        while (true)
        {
            acceptConnection(m_new_socket);
            log("------ Client connected ------");

            // Read the client request
            const int BUFFER_SIZE = 30720;
            char buffer[BUFFER_SIZE] = { 0 };
            long bytesReceived = read(m_new_socket, buffer, BUFFER_SIZE);

            if (bytesReceived < 0)
            {
                exitWithError("Failed to read bytes from client socket connection");
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
                long bytesSent = write(m_new_socket, m_serverMessage.c_str(), m_serverMessage.size());
                if (bytesSent == static_cast<long>(m_serverMessage.size()))
                {
                    log("------ Response sent to client ------\n");
                }
                else
                {
                    log("Error sending response to the client.");
                }
            }

            // Close this connected socket
            close(m_new_socket);
            m_new_socket = -1;
        }
    }

    void TcpServer::acceptConnection(int& new_socket)
    {
        new_socket = accept(m_socket,
            reinterpret_cast<struct sockaddr*>(&m_socketAddress),
            &m_socketAddress_len);
        if (new_socket < 0)
        {
            std::ostringstream ss;
            ss << "Server failed to accept incoming connection from ADDRESS: "
                << inet_ntoa(m_socketAddress.sin_addr)
                << "; PORT: " << ntohs(m_socketAddress.sin_port);

            exitWithError(ss.str());
        }
    }

    void TcpServer::closeServer()
    {
        // Close the main listening socket
        if (m_socket >= 0)
        {
            close(m_socket);
            m_socket = -1;
        }

        // Close the connection socket if open
        if (m_new_socket >= 0)
        {
            close(m_new_socket);
            m_new_socket = -1;
        }
    }

    void TcpServer::log(const std::string& message)
    {
        std::cout << message << std::endl;
    }

    void TcpServer::exitWithError(const std::string& errorMessage)
    {
        std::cerr << "ERROR: " << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
*/