#include "TwitchEventSub.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

TwitchEventSub::TwitchEventSub()
    : m_hSession(nullptr)
    , m_hConnection(nullptr)
    , m_hRequest(nullptr)
    , m_hWebSocket(nullptr)
    , m_connected(false)
    , m_running(false)
    , m_disconnecting(false)
{
}

TwitchEventSub::~TwitchEventSub()
{
    Disconnect();
}

void TwitchEventSub::SetCredentials(const std::string& clientId, const std::string& userId, const std::string& accessToken)
{
    m_clientId = clientId;
    m_userId = userId;
    m_accessToken = accessToken;
}

void TwitchEventSub::SetOnFollowCallback(std::function<void(const std::string&)> callback)
{
    m_onFollow = callback;
}

void TwitchEventSub::SetOnChannelPointRedemptionCallback(std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    m_onChannelPointRedemption = callback;
}

void TwitchEventSub::SetRewardNameFilter(const std::string& rewardName)
{
    m_rewardNameFilter = rewardName;
    std::cout << "[EventSub] Channel point reward filter set to: " << rewardName << std::endl;
}

bool TwitchEventSub::Connect()
{
    if (m_connected) {
        std::cout << "[EventSub] Already connected" << std::endl;
        return true;
    }

    std::cout << "[EventSub] Connecting to Twitch EventSub WebSocket..." << std::endl;

    if (!ConnectWebSocket()) {
        std::cout << "[EventSub] Failed to connect WebSocket" << std::endl;
        return false;
    }

    m_connected = true;
    m_running = true;

    // Start receive thread
    // Join old thread if it exists
    if (m_receiveThread.joinable()) {
        std::cout << "[EventSub] Joining old receive thread..." << std::endl;
        m_running = false;
        m_receiveThread.join();
        m_running = true;
    }

    m_receiveThread = std::thread(&TwitchEventSub::ReceiveLoop, this);

    return true;
}

void TwitchEventSub::Disconnect()
{
    // Atomic check-and-set to prevent concurrent disconnects
    bool expected = false;
    if (!m_disconnecting.compare_exchange_strong(expected, true)) {
        std::cout << "[EventSub] Disconnect already in progress by another thread, ignoring" << std::endl;
        return;
    }

    if (!m_connected) {
        std::cout << "[EventSub] Already disconnected" << std::endl;
        m_disconnecting = false;
        return;
    }

    std::cout << "[EventSub] Disconnecting..." << std::endl;

    m_running = false;
    m_connected = false;

    // Close WebSocket
    if (m_hWebSocket) {
        WinHttpWebSocketClose(m_hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        WinHttpCloseHandle(m_hWebSocket);
        m_hWebSocket = nullptr;
    }

    // Close WinHTTP handles
    if (m_hRequest) {
        WinHttpCloseHandle(m_hRequest);
        m_hRequest = nullptr;
    }
    if (m_hConnection) {
        WinHttpCloseHandle(m_hConnection);
        m_hConnection = nullptr;
    }
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
    }

    if (m_receiveThread.joinable()) {
        std::cout << "[EventSub] Joining receive thread..." << std::endl;
        m_receiveThread.join();
        std::cout << "[EventSub] Receive thread joined" << std::endl;
    }

    std::cout << "[EventSub] Disconnect complete" << std::endl;
    m_disconnecting = false;  // Reset flag
}

bool TwitchEventSub::ConnectWebSocket()
{
    std::cout << "[EventSub] Connecting to Twitch EventSub WebSocket..." << std::endl;

    // Step 1: Initialize WinHTTP session
    m_hSession = WinHttpOpen(
        L"GobTalk EventSub/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!m_hSession) {
        std::cout << "[EventSub] Failed to open WinHTTP session" << std::endl;
        return false;
    }

    // Step 2: Connect to server
    m_hConnection = WinHttpConnect(
        m_hSession,
        L"eventsub.wss.twitch.tv",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (!m_hConnection) {
        std::cout << "[EventSub] Failed to connect to server" << std::endl;
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
        return false;
    }

    // Step 3: Open HTTP request
    m_hRequest = WinHttpOpenRequest(
        m_hConnection,
        L"GET",
        L"/ws",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE  // HTTPS/WSS
    );

    if (!m_hRequest) {
        std::cout << "[EventSub] Failed to open request" << std::endl;
        WinHttpCloseHandle(m_hConnection);
        WinHttpCloseHandle(m_hSession);
        m_hConnection = nullptr;
        m_hSession = nullptr;
        return false;
    }

    // Step 4: Set WebSocket option (must be set BEFORE sending request)
    BOOL bResult = WinHttpSetOption(
        m_hRequest,
        WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
        nullptr,
        0
    );

    if (!bResult) {
        std::cout << "[EventSub] Failed to set WebSocket option" << std::endl;
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnection);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = nullptr;
        m_hConnection = nullptr;
        m_hSession = nullptr;
        return false;
    }

    // Step 5: Send HTTP request to initiate WebSocket handshake
    bResult = WinHttpSendRequest(
        m_hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    if (!bResult) {
        std::cout << "[EventSub] Failed to send request" << std::endl;
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnection);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = nullptr;
        m_hConnection = nullptr;
        m_hSession = nullptr;
        return false;
    }

    // Step 6: Receive response
    bResult = WinHttpReceiveResponse(m_hRequest, nullptr);

    if (!bResult) {
        std::cout << "[EventSub] Failed to receive response" << std::endl;
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnection);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = nullptr;
        m_hConnection = nullptr;
        m_hSession = nullptr;
        return false;
    }

    // Step 7: Complete upgrade to WebSocket
    m_hWebSocket = WinHttpWebSocketCompleteUpgrade(m_hRequest, 0);

    if (!m_hWebSocket) {
        std::cout << "[EventSub] Failed to upgrade to WebSocket" << std::endl;
        WinHttpCloseHandle(m_hRequest);
        WinHttpCloseHandle(m_hConnection);
        WinHttpCloseHandle(m_hSession);
        m_hRequest = nullptr;
        m_hConnection = nullptr;
        m_hSession = nullptr;
        return false;
    }

    // Request handle is no longer needed after WebSocket upgrade
    WinHttpCloseHandle(m_hRequest);
    m_hRequest = nullptr;

    std::cout << "[EventSub] WebSocket connection established successfully" << std::endl;
    return true;
}

void TwitchEventSub::ReceiveLoop()
{
    std::cout << "[EventSub] Receive loop started" << std::endl;

    while (m_running) {
        std::string message = ReceiveWebSocketFrame();

        if (message.empty()) {
            if (!m_running) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ProcessMessage(message);
    }

    std::cout << "[EventSub] Receive loop ended" << std::endl;
}

std::string TwitchEventSub::ReceiveWebSocketFrame()
{
    if (!m_hWebSocket) {
        return "";
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;

    DWORD error = WinHttpWebSocketReceive(
        m_hWebSocket,
        buffer,
        sizeof(buffer),
        &bytesRead,
        &bufferType
    );

    if (error != NO_ERROR) {
        if (error == ERROR_INVALID_OPERATION) {
            // WebSocket closed
            return "";
        }
        std::cout << "[EventSub] WebSocket receive error: " << error << std::endl;
        return "";
    }

    // Handle different buffer types
    if (bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
        std::cout << "[EventSub] WebSocket close frame received" << std::endl;
        return "";
    }

    return std::string(buffer, bytesRead);
}

bool TwitchEventSub::SendWebSocketFrame(const std::string& data)
{
    if (!m_hWebSocket) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_websocketMutex);

    DWORD error = WinHttpWebSocketSend(
        m_hWebSocket,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        (PVOID)data.c_str(),
        (DWORD)data.length()
    );

    if (error != NO_ERROR) {
        std::cout << "[EventSub] WebSocket send error: " << error << std::endl;
        return false;
    }

    return true;
}

void TwitchEventSub::ProcessMessage(const std::string& message)
{
    std::cout << "[EventSub] Received: " << message << std::endl;

    // Extract message type
    std::string messageType = ExtractNestedJSONValue(message, "metadata>message_type");

    if (messageType == "session_welcome") {
        HandleWelcomeMessage(message);
    }
    else if (messageType == "session_keepalive") {
        HandleKeepaliveMessage();
    }
    else if (messageType == "notification") {
        HandleNotificationMessage(message);
    }
}

void TwitchEventSub::HandleWelcomeMessage(const std::string& payload)
{
    std::cout << "[EventSub] Received welcome message" << std::endl;

    // Extract session ID
    m_sessionId = ExtractNestedJSONValue(payload, "payload>session>id");

    if (m_sessionId.empty()) {
        std::cout << "[EventSub] Failed to extract session ID" << std::endl;
        return;
    }

    std::cout << "[EventSub] Session ID: " << m_sessionId << std::endl;

    // Subscribe to follow events
    if (!SubscribeToFollows()) {
        std::cout << "[EventSub] Failed to subscribe to follow events" << std::endl;
    }

    // Subscribe to channel point redemptions
    if (!SubscribeToChannelPointRedemptions()) {
        std::cout << "[EventSub] Failed to subscribe to channel point redemptions" << std::endl;
    }
}

void TwitchEventSub::HandleNotificationMessage(const std::string& payload)
{
    // Extract event type
    std::string eventType = ExtractNestedJSONValue(payload, "payload>subscription>type");

    if (eventType == "channel.follow") {
        // Extract follower username
        std::string username = ExtractNestedJSONValue(payload, "payload>event>user_name");

        std::cout << "[EventSub] New follower: " << username << std::endl;

        if (m_onFollow && !username.empty()) {
            m_onFollow(username);
        }
    }
    else if (eventType == "channel.channel_points_custom_reward_redemption.add") {
        // Extract redemption details
        std::string username = ExtractNestedJSONValue(payload, "payload>event>user_name");
        std::string rewardTitle = ExtractNestedJSONValue(payload, "payload>event>reward>title");
        std::string userInput = ExtractNestedJSONValue(payload, "payload>event>user_input");

        std::cout << "[EventSub] Channel point redemption: " << username << " redeemed '" << rewardTitle << "'" << std::endl;
        if (!userInput.empty()) {
            std::cout << "[EventSub] User input: " << userInput << std::endl;
        }

        // Check if reward matches filter (if filter is set)
        if (!m_rewardNameFilter.empty()) {
            if (rewardTitle != m_rewardNameFilter) {
                std::cout << "[EventSub] Reward doesn't match filter '" << m_rewardNameFilter << "', ignoring" << std::endl;
                return;
            }
        }

        if (m_onChannelPointRedemption && !username.empty() && !rewardTitle.empty()) {
            m_onChannelPointRedemption(username, rewardTitle, userInput);
        }
    }
}

void TwitchEventSub::HandleKeepaliveMessage()
{
    // Just acknowledge - no action needed
}

bool TwitchEventSub::SubscribeToFollows()
{
    if (m_sessionId.empty() || m_userId.empty() || m_accessToken.empty()) {
        std::cout << "[EventSub] Missing credentials for subscription" << std::endl;
        return false;
    }

    std::cout << "[EventSub] Subscribing to follow events..." << std::endl;

    // Build subscription JSON
    std::stringstream jsonData;
    jsonData << "{";
    jsonData << "\"type\":\"channel.follow\",";
    jsonData << "\"version\":\"2\",";
    jsonData << "\"condition\":{";
    jsonData << "\"broadcaster_user_id\":\"" << m_userId << "\",";
    jsonData << "\"moderator_user_id\":\"" << m_userId << "\"";
    jsonData << "},";
    jsonData << "\"transport\":{";
    jsonData << "\"method\":\"websocket\",";
    jsonData << "\"session_id\":\"" << m_sessionId << "\"";
    jsonData << "}";
    jsonData << "}";

    std::string response = HttpPostJSON("https://api.twitch.tv/helix/eventsub/subscriptions", jsonData.str());

    if (response.find("\"status\":\"enabled\"") != std::string::npos ||
        response.find("\"status\":\"websocket_") != std::string::npos) {
        std::cout << "[EventSub] Successfully subscribed to follow events" << std::endl;
        return true;
    }

    std::cout << "[EventSub] Subscription failed: " << response << std::endl;
    return false;
}

bool TwitchEventSub::SubscribeToChannelPointRedemptions()
{
    if (m_sessionId.empty() || m_userId.empty() || m_accessToken.empty()) {
        std::cout << "[EventSub] Missing credentials for subscription" << std::endl;
        return false;
    }

    std::cout << "[EventSub] Subscribing to channel point redemptions..." << std::endl;

    // Build subscription JSON
    std::stringstream jsonData;
    jsonData << "{";
    jsonData << "\"type\":\"channel.channel_points_custom_reward_redemption.add\",";
    jsonData << "\"version\":\"1\",";
    jsonData << "\"condition\":{";
    jsonData << "\"broadcaster_user_id\":\"" << m_userId << "\"";
    jsonData << "},";
    jsonData << "\"transport\":{";
    jsonData << "\"method\":\"websocket\",";
    jsonData << "\"session_id\":\"" << m_sessionId << "\"";
    jsonData << "}";
    jsonData << "}";

    std::string response = HttpPostJSON("https://api.twitch.tv/helix/eventsub/subscriptions", jsonData.str());

    if (response.find("\"status\":\"enabled\"") != std::string::npos ||
        response.find("\"status\":\"websocket_") != std::string::npos) {
        std::cout << "[EventSub] Successfully subscribed to channel point redemptions" << std::endl;
        return true;
    }

    std::cout << "[EventSub] Subscription failed: " << response << std::endl;
    return false;
}

std::string TwitchEventSub::HttpPostJSON(const std::string& url, const std::string& jsonData)
{
    // Parse URL
    std::wstring wideUrl(url.begin(), url.end());

    HINTERNET hSession = WinHttpOpen(L"TwitchEventSub/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t host[256], path[256];
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host) / sizeof(wchar_t);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path) / sizeof(wchar_t);

    WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &urlComp);

    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Headers
    std::wstring authHeader = L"Authorization: Bearer " + std::wstring(m_accessToken.begin(), m_accessToken.end());
    std::wstring clientIdHeader = L"Client-Id: " + std::wstring(m_clientId.begin(), m_clientId.end());
    std::wstring contentType = L"Content-Type: application/json";

    WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, clientIdHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, contentType.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    BOOL result = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)jsonData.c_str(), (DWORD)jsonData.length(), (DWORD)jsonData.length(), 0);

    std::string response;
    if (result && WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD size = 0;
        char buffer[4096];
        do {
            size = 0;
            if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
                size = min(size, sizeof(buffer));
                DWORD downloaded = 0;
                if (WinHttpReadData(hRequest, buffer, size, &downloaded)) {
                    response.append(buffer, downloaded);
                }
            }
        } while (size > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string TwitchEventSub::ExtractJSONValue(const std::string& json, const std::string& key)
{
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    pos += searchKey.length();
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";

    return json.substr(pos, endPos - pos);
}

std::string TwitchEventSub::ExtractNestedJSONValue(const std::string& json, const std::string& path)
{
    // Simple nested JSON extraction: "metadata>message_type" -> find "metadata", then "message_type"
    std::string current = json;
    std::stringstream ss(path);
    std::string key;

    while (std::getline(ss, key, '>')) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = current.find(searchKey);
        if (pos == std::string::npos) return "";

        pos += searchKey.length();

        // Skip whitespace
        while (pos < current.length() && (current[pos] == ' ' || current[pos] == '\n' || current[pos] == '\r')) pos++;

        if (current[pos] == '{') {
            // Nested object - find matching closing brace
            int braceCount = 1;
            size_t start = pos + 1;
            pos++;
            while (pos < current.length() && braceCount > 0) {
                if (current[pos] == '{') braceCount++;
                else if (current[pos] == '}') braceCount--;
                pos++;
            }
            current = current.substr(start, pos - start - 1);
        }
        else if (current[pos] == '"') {
            // String value
            pos++;
            size_t endPos = current.find('"', pos);
            if (endPos == std::string::npos) return "";
            return current.substr(pos, endPos - pos);
        }
    }

    return "";
}