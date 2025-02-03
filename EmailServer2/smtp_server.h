#ifndef INCLUDED_SMTP_TCPSERVER_LINUX
#define INCLUDED_SMTP_TCPSERVER_LINUX

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <arpa/inet.h>
#include <openssl/ssl.h> // For future TLS integration

namespace smtp {
    class TcpServer {
    public:
        TcpServer(const std::string& ipAddress = "0.0.0.0", int port = 25, int maxThreads = 50);
        ~TcpServer();
        void startListen();

    private:
        // SMTP Protocol Handlers
        void handleClient(int clientSocket);
        void sendResponse(int socket, const std::string& response);
        bool validateEmail(const std::string& email); // Basic RFC 5322 validation

        // Security
        void sanitizeInput(std::string& data);
        bool rateLimitCheck(const sockaddr_in& clientAddr); // Limits 10 requests/sec per IP

        // Thread Pool
        std::vector<std::thread> workerThreads;
        std::queue<int> clientQueue;
        std::mutex queueMutex;
        std::condition_variable condition;
        bool shutdownFlag = false;

        // Server State
        int m_socket;
        struct sockaddr_in m_socketAddress;
        std::string m_ip_address;
        int m_port;

        // Security Metrics
        std::atomic<int> blockedRequests{ 0 };
        std::atomic<int> emailsProcessed{ 0 };

        // Database 
        sqlite3* m_db;
    };
}

#endif