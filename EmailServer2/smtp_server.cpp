#include "smtp_server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex>

namespace smtp {
    TcpServer::TcpServer(const std::string& ipAddress, int port, int maxThreads)
        : m_ip_address(ipAddress), m_port(port) {
        // Initialize thread pool
        for (int i = 0; i < maxThreads; ++i) {
            workerThreads.emplace_back([this] {
                while (true) {
                    int clientSocket = -1;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] {
                            return !clientQueue.empty() || shutdownFlag;
                            });
                        if (shutdownFlag) return;
                        clientSocket = clientQueue.front();
                        clientQueue.pop();
                    }
                    handleClient(clientSocket);
                }
                });
        }

        // Inside TcpServer::TcpServer(...) constructor:

// Create the socket
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0) {
            exitWithError("Failed to create socket");
        }

        // Allow socket reuse to avoid "address already in use" errors
        int opt = 1;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            exitWithError("Failed to set SO_REUSEADDR");
        }

        // Configure the server address structure
        struct sockaddr_in serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(m_port); // SMTP default: port 25 (requires sudo)
        serverAddress.sin_addr.s_addr = inet_addr(m_ip_address.c_str());

        // Bind the socket to the IP/port
        if (bind(m_socket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::ostringstream ss;
            ss << "Failed to bind to " << m_ip_address << ":" << m_port;
            exitWithError(ss.str());
        }

        // Start listening (backlog of 100 pending connections)
        if (listen(m_socket, 100) < 0) {
            exitWithError("Failed to listen on socket");
        }

        // Log server start
        std::ostringstream ss;
        ss << "SMTP server started on " << m_ip_address << ":" << m_port;
        log(ss.str());
    }

    TcpServer::~TcpServer() {
        shutdownFlag = true;
        condition.notify_all();
        for (auto& thread : workerThreads) {
            if (thread.joinable()) thread.join();
        }
        close(m_socket);
    }

    void TcpServer::handleClient(int clientSocket) {
        // SMTP State Machine
        enum class SmtpState { INIT, HELO, MAIL, RCPT, DATA, QUIT };
        SmtpState state = SmtpState::INIT;
        std::string clientData;

        sendResponse(clientSocket, "220 smtp.example.com ESMTP Ready\r\n");

        char buffer[1024];
        while (true) {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) break;

            clientData.append(buffer, bytesRead);
            sanitizeInput(clientData); // Remove suspicious characters

            // Process SMTP commands
            if (clientData.find("\r\n") != std::string::npos) {
                std::istringstream iss(clientData);
                std::string command;
                while (std::getline(iss, command, '\r\n')) {
                    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

                    if (command.substr(0, 4) == "HELO" && state == SmtpState::INIT) {
                        sendResponse(clientSocket, "250 Hello " + command.substr(5) + "\r\n");
                        state = SmtpState::HELO;
                    }
                    else if (command.substr(0, 4) == "MAIL" && state == SmtpState::HELO) {
                        if (validateEmail(command.substr(10))) { // Extract FROM
                            sendResponse(clientSocket, "250 Sender OK\r\n");
                            state = SmtpState::MAIL;
                        }
                        else {
                            sendResponse(clientSocket, "550 Invalid sender\r\n");
                        }
                    }
                    else if (command.substr(0, 4) == "RCPT" && state == SmtpState::MAIL) {
                        std::string recipient = command.substr(8); // "RCPT TO:<user@domain.com>"
                        if (validateEmail(recipient)) {
                            sendResponse(clientSocket, "250 Recipient OK\r\n");
                            state = SmtpState::RCPT;
                        }
                        else {
                            sendResponse(clientSocket, "550 Invalid recipient\r\n");
                        }
                    }
                    else if (command == "DATA" && state == SmtpState::RCPT) {
                        sendResponse(clientSocket, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
                        std::string emailBody;
                        while (true) {
                            bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                            emailBody.append(buffer, bytesRead);
                            // Terminate on "<CRLF>.<CRLF>"
                            if (emailBody.find("\r\n.\r\n") != std::string::npos) break;
                        }
                        sendResponse(clientSocket, "250 Message accepted for delivery\r\n");
                        state = SmtpState::DATA;
                    }
                    else if (command == "QUIT") {
                        sendResponse(clientSocket, "221 Bye\r\n");
                        close(clientSocket);
                        state = SmtpState::QUIT;
                        break; // Exit the loop
                    }
                }
            }
        }
        close(clientSocket);
        emailsProcessed++;
    }

    bool TcpServer::validateEmail(const std::string& email) {
        // Basic RFC 5322 regex check
        // More RFC 5322-compliant regex (still simplified):
        const std::regex pattern(R"(^([a-zA-Z0-9_\-\.\+]+)@([a-zA-Z0-9_\-\.]+)\.([a-zA-Z]{2,})$)");
        return std::regex_match(email, pattern);
    }

    void TcpServer::sanitizeInput(std::string& data) {
        // Prevent CRLF injection and strip non-printable chars
        data.erase(std::remove_if(data.begin(), data.end(),
            [](char c) { return !isprint(c) || c == '\r' || c == '\n'; }), data.end());
    }

    bool checkSpam(const std::string& emailBody) {
        // Connect to Python spam service
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(65432);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Failed to connect to spam filter" << std::endl;
            return false;
        }

        // Send email body to Python
        send(sock, emailBody.c_str(), emailBody.size(), 0);

        // Receive result (SPAM/HAM)
        char buffer[4] = { 0 };
        recv(sock, buffer, 4, 0);
        close(sock);
        return std::string(buffer) == "SPAM";
    }

    // In handleClient():
    if (checkSpam(emailBody)) {
        sendResponse(clientSocket, "554 Message rejected as spam\r\n");
    }
    else {
        
    }
}