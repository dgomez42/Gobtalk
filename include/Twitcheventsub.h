#pragma once
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <windows.h>
#include <winhttp.h>

class TwitchEventSub {
public:
    TwitchEventSub();
    ~TwitchEventSub();

    // Setup
    void SetCredentials(const std::string& clientId, const std::string& userId, const std::string& accessToken);

    // Connection
    bool Connect();
    void Disconnect();
    bool IsConnected() const { return m_connected; }
    bool IsDisconnecting() const { return m_disconnecting.load(); }

    // Callbacks
    void SetOnFollowCallback(std::function<void(const std::string& username)> callback);
    void SetOnChannelPointRedemptionCallback(std::function<void(const std::string& username, const std::string& rewardTitle, const std::string& userInput)> callback);
    void SetRewardNameFilter(const std::string& rewardName);

private:
    // Credentials
    std::string m_clientId;
    std::string m_userId;
    std::string m_accessToken;

    // WinHTTP WebSocket connection
    HINTERNET m_hSession;
    HINTERNET m_hConnection;
    HINTERNET m_hRequest;
    HINTERNET m_hWebSocket;
    bool m_connected;
    bool m_running;
    std::thread m_receiveThread;
    std::mutex m_websocketMutex;
    std::atomic<bool> m_disconnecting;

    // EventSub session
    std::string m_sessionId;

    // Reward name filter
    std::string m_rewardNameFilter;

    // Callbacks
    std::function<void(const std::string&)> m_onFollow;
    std::function<void(const std::string&, const std::string&, const std::string&)> m_onChannelPointRedemption;

    // Connection methods
    bool ConnectWebSocket();
    void ReceiveLoop();
    void ProcessMessage(const std::string& message);
    void HandleWelcomeMessage(const std::string& payload);
    void HandleNotificationMessage(const std::string& payload);
    void HandleKeepaliveMessage();

    // Subscription methods
    bool SubscribeToFollows();
    bool SubscribeToChannelPointRedemptions();

    // HTTP helpers
    std::string HttpPostJSON(const std::string& url, const std::string& jsonData);

    // WebSocket helpers
    bool SendWebSocketFrame(const std::string& data);
    std::string ReceiveWebSocketFrame();

    // JSON parsing helpers (simple)
    std::string ExtractJSONValue(const std::string& json, const std::string& key);
    std::string ExtractNestedJSONValue(const std::string& json, const std::string& path);
};