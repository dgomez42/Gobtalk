#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <WinSock2.h>

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct ChatMessage {
    std::string username;
    std::string message;
    std::string timestamp;
};

class TwitchAuth {
public:
    TwitchAuth();
    ~TwitchAuth();

    // Configuration
    void SetClientId(const std::string& clientId) { m_clientId = clientId; }
    void SetWorkerUrl(const std::string& workerUrl) { m_workerUrl = workerUrl; }

    // Authentication
    bool IsAuthenticated() const;
    bool StartAuthentication();  // Opens browser, starts local server
    bool LoadUserConfig();       // Load saved user_id
    bool SaveUserConfig();       // Save user_id
    void Disconnect();
    void RevokeAuthentication(); // Remove user from worker

    // IRC Connection
    bool Connect(const std::string& channel);
    bool SendMessage(const std::string& message);

    // Token management (handled by worker)
    std::string GetAccessToken(); // Gets valid token from worker API

    // Status
    ConnectionStatus GetStatus() const { return m_status; }
    std::string GetStatusMessage() const { return m_statusMessage; }
    std::string GetUsername() const { return m_username; }
    std::string GetUserId() const { return m_userId; }

    // Messages
    std::vector<ChatMessage> GetMessages() const;
    void SetOnMessageCallback(std::function<void(const ChatMessage&)> callback);
    void SetOnStatusCallback(std::function<void(ConnectionStatus, const std::string&)> callback);

private:
    // Configuration
    std::string m_clientId;
    std::string m_workerUrl;
    std::string m_userId;
    std::string m_username;
    std::string m_channel;

    // IRC Connection
    SOCKET m_socket;
    std::thread m_receiveThread;
    bool m_running;

    // Local HTTP server for OAuth callback
    SOCKET m_serverSocket;
    std::thread m_serverThread;
    bool m_serverRunning;
    void StartLocalServer();
    void StopLocalServer();
    void ServerLoop();

    // Status
    ConnectionStatus m_status;
    std::string m_statusMessage;
    void SetStatus(ConnectionStatus status, const std::string& message);

    // Messages
    std::vector<ChatMessage> m_messages;
    mutable std::mutex m_messagesMutex;
    std::function<void(const ChatMessage&)> m_onMessage;
    std::function<void(ConnectionStatus, const std::string&)> m_onStatus;
    static constexpr size_t MAX_MESSAGES = 1000;

    // IRC Processing
    void ReceiveLoop();
    void ProcessLine(const std::string& line);
    void ParseMessage(const std::string& line);

    // Socket helpers
    bool InitializeSocket();
    void CleanupSocket();

    // HTTP helpers
    std::string HttpGet(const std::string& url);
    std::string HttpGetWithAuth(const std::string& url, const std::string& token);

    // Utility
    std::string GetCurrentTimestamp();
};