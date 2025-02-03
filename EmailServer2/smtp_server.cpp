#include "smtp_server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex>
#include <sqlite3.h>

namespace smtp {



// Constructor
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




// Database Initialization
        sqlite3* db;


        if (sqlite3_open("smtp_server.db", &db) != SQLITE_OK) {
            std::cerr << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }

        // Create tables if they don't exist
        const char* createTablesSQL = R"(
    CREATE TABLE IF NOT EXISTS Emails (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        sender TEXT NOT NULL,
        recipient TEXT NOT NULL,
        subject TEXT,
        body TEXT NOT NULL,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        status TEXT DEFAULT 'QUEUED',
        spam_score REAL
    );
    CREATE TABLE IF NOT EXISTS SpamLogs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        sender TEXT NOT NULL,
        recipient TEXT NOT NULL,
        body TEXT NOT NULL,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        spam_score REAL NOT NULL
    );
)";
        if (sqlite3_exec(db, createTablesSQL, nullptr, nullptr, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to create tables: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }
        sqlite3_close(db);





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




// Destructor
    TcpServer::~TcpServer() {
        shutdownFlag = true;
        condition.notify_all();
        for (auto& thread : workerThreads) {
            if (thread.joinable()) thread.join();
        }
        close(m_socket);
        sqlite3_close(m_db);
    }



// Start listening for connections 
    void TcpServer::startListen() {
        while (!shutdownFlag) {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int clientSocket = accept(m_socket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket < 0) {
                if (shutdownFlag) break; // Graceful shutdown
                exitWithError("Failed to accept connection");
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                clientQueue.push(clientSocket);
            }
            condition.notify_one();
        }
    }




// Email Processing
    void TcpServer::handleClient(int clientSocket) {
        // SMTP State Machine
        enum class SmtpState { INIT, HELO, MAIL, RCPT, DATA, QUIT };
        SmtpState state = SmtpState::INIT;
        std::string sender, recipient, emailBody;
        bool connectionActive = true;

        sendResponse(clientSocket, "220 smtp.example.com ESMTP Ready\r\n");

        char buffer[1024];
        while (connectionActive) {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) break;

            std::string clientData(buffer, bytesRead);
            sanitizeInput(clientData);

            // Process complete SMTP commands (CRLF separated)
            size_t crlfPos;
            while ((crlfPos = clientData.find("\r\n")) != std::string::npos) {
                std::string command = clientData.substr(0, crlfPos);
                clientData.erase(0, crlfPos + 2);
                std::transform(command.begin(), command.end(), command.begin(), ::toupper);

                try {
                    switch (state) {
                    case SmtpState::INIT:
                        if (command.substr(0, 4) == "HELO") {
                            sendResponse(clientSocket, "250 Hello " + command.substr(5) + "\r\n");
                            state = SmtpState::HELO;
                        }
                        break;

                    case SmtpState::HELO:
                        if (command.substr(0, 4) == "MAIL") {
                            sender = extractEmailAddress(command.substr(10));
                            if (validateEmail(sender)) {
                                sendResponse(clientSocket, "250 Sender OK\r\n");
                                state = SmtpState::MAIL;
                            }
                            else {
                                sendResponse(clientSocket, "550 Invalid sender address\r\n");
                            }
                        }
                        break;

                    case SmtpState::MAIL:
                        if (command.substr(0, 4) == "RCPT") {
                            recipient = extractEmailAddress(command.substr(8));
                            if (validateEmail(recipient)) {
                                sendResponse(clientSocket, "250 Recipient OK\r\n");
                                state = SmtpState::RCPT;
                            }
                            else {
                                sendResponse(clientSocket, "550 Invalid recipient address\r\n");
                            }
                        }
                        break;

                    case SmtpState::RCPT:
                        if (command == "DATA") {
                            sendResponse(clientSocket, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
                            state = SmtpState::DATA;
                        }
                        break;

                    case SmtpState::DATA: {
                        // Handle dot-stuffing (RFC 5321 Section 4.5.2)
                        size_t dotPos = emailBody.find("\r\n.");
                        while (dotPos != std::string::npos) {
                            emailBody.replace(dotPos, 3, "\r\n");
                            dotPos = emailBody.find("\r\n.", dotPos + 2);
                        }

                        // Check for termination sequence
                        if (emailBody.find("\r\n.\r\n") != std::string::npos) {
                            // Process complete email
                            bool isSpam = checkSpam(emailBody);

                            if (isSpam) {
                                logSpam(sender, recipient, emailBody);
                                sendResponse(clientSocket, "554 Message rejected as spam\r\n");
                            }
                            else {
                                storeEmail(sender, recipient, emailBody);
                                sendResponse(clientSocket, "250 Message accepted for delivery\r\n");
                            }

                            // Reset for next email
                            state = SmtpState::HELO;
                            emailBody.clear();
                        }
                        break;
                    }

                    case SmtpState::QUIT:
                        sendResponse(clientSocket, "221 Bye\r\n");
                        close(clientSocket);
                        connectionActive = false;
                        break;
                    }
                }
                catch (const std::out_of_range&) {
                    sendResponse(clientSocket, "500 Syntax error, command unrecognized\r\n");
                }

                // Handle QUIT command in any state
                if (command == "QUIT") {
                    state = SmtpState::QUIT;
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




    std::string TcpServer::extractEmailAddress(const std::string& input) {
        size_t start = input.find('<');
        size_t end = input.find('>');
        if (start != std::string::npos && end != std::string::npos) {
            return input.substr(start + 1, end - start - 1);
        }
        return input;
    }




    void TcpServer::storeEmail(const std::string& sender,
        const std::string& recipient,
        const std::string& body) {
        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO Emails (sender, recipient, body) VALUES (?, ?, ?);";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, recipient.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, body.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                log("Database error: " + std::string(sqlite3_errmsg(m_db)));
            }
            sqlite3_finalize(stmt);
        }
    }




}