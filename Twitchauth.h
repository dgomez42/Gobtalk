#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <set>
#include <atomic>
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
    ConnectionStatus GetStatus() const {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        return m_status;
    }
    std::string GetStatusMessage() const {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        return m_statusMessage;
    }
    std::string GetUsername() const {
        std::lock_guard<std::mutex> lock(m_authMutex);
        return m_username;
    }
    std::string GetUserId() const {
        std::lock_guard<std::mutex> lock(m_authMutex);
        return m_userId;
    }
    std::string GetClientId() const { return m_clientId; }
    bool IsDisconnecting() const { return m_disconnecting.load(); }

    // Messages
    std::vector<ChatMessage> GetMessages() const;
    void SetOnMessageCallback(std::function<void(const ChatMessage&)> callback);
    void SetOnRawMessageCallback(std::function<void(const std::string&)> callback);
    void SetOnStatusCallback(std::function<void(ConnectionStatus, const std::string&)> callback);
    void SetOnNewFollowerCallback(std::function<void(const std::string&)> callback);

    // Follower tracking
    void StartFollowerPolling();
    void StopFollowerPolling();

private:
    // Configuration
    std::string m_clientId;
    std::string m_workerUrl;
    std::string m_userId;
    std::string m_username;
    std::string m_channel;
    mutable std::mutex m_authMutex;  // Protect m_userId and m_username

    // IRC Connection
    SOCKET m_socket;
    std::thread m_receiveThread;
    bool m_running;
    std::atomic<bool> m_disconnecting;

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
    mutable std::mutex m_statusMutex;  // Protect status variables
    void SetStatus(ConnectionStatus status, const std::string& message);

    // Messages
    std::vector<ChatMessage> m_messages;
    mutable std::mutex m_messagesMutex;
    std::function<void(const ChatMessage&)> m_onMessage;
    std::function<void(const std::string&)> m_onRawMessage;
    std::function<void(ConnectionStatus, const std::string&)> m_onStatus;
    std::function<void(const std::string&)> m_onNewFollower;
    static constexpr size_t MAX_MESSAGES = 1000;

    // Follower tracking
    std::thread m_followerPollThread;
    bool m_followerPolling;
    std::set<std::string> m_knownFollowers; // Store follower user IDs
    mutable std::mutex m_followerMutex;
    void FollowerPollLoop();
    void CheckForNewFollowers();

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