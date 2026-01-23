#include "TwitchAuth.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <shellapi.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

// Configuration - update these with your values
constexpr const char* DEFAULT_CLIENT_ID = "7ss0a268wocpa77eduq9vsgv0aiwu6";
constexpr const char* DEFAULT_WORKER_URL = "https://gobtalk.lewistittle567.workers.dev";
constexpr const char* USER_CONFIG_FILE = "twitch_config.json";
constexpr int CALLBACK_SERVER_PORT = 3000;

TwitchAuth::TwitchAuth()
    : m_clientId(DEFAULT_CLIENT_ID)
    , m_workerUrl(DEFAULT_WORKER_URL)
    , m_status(ConnectionStatus::Disconnected)
    , m_running(false)
    , m_socket(INVALID_SOCKET)
    , m_serverSocket(INVALID_SOCKET)
    , m_serverRunning(false)
{
    // Initialize WinSock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Try to load saved user config
    LoadUserConfig();
}

TwitchAuth::~TwitchAuth()
{
    Disconnect();
    StopLocalServer();
    WSACleanup();
}

bool TwitchAuth::LoadUserConfig()
{
    std::cout << "[TwitchAuth] Loading user config from " << USER_CONFIG_FILE << std::endl;
    std::ifstream file(USER_CONFIG_FILE);
    if (!file.is_open()) {
        std::cout << "[TwitchAuth] Config file not found" << std::endl;
        return false;
    }

    // Simple JSON parsing for user_id and username
    std::string content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    std::cout << "[TwitchAuth] Config content: " << content << std::endl;

    // Parse "user_id": "value"
    size_t userIdPos = content.find("\"user_id\"");
    if (userIdPos != std::string::npos) {
        size_t valueStart = content.find("\"", userIdPos + 10);
        size_t valueEnd = content.find("\"", valueStart + 1);
        if (valueStart != std::string::npos && valueEnd != std::string::npos) {
            m_userId = content.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    }

    // Parse "username": "value"
    size_t usernamePos = content.find("\"username\"");
    if (usernamePos != std::string::npos) {
        size_t valueStart = content.find("\"", usernamePos + 11);
        size_t valueEnd = content.find("\"", valueStart + 1);
        if (valueStart != std::string::npos && valueEnd != std::string::npos) {
            m_username = content.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    }

    std::cout << "[TwitchAuth] Loaded user_id: " << m_userId << ", username: " << m_username << std::endl;
    return !m_userId.empty();
}

bool TwitchAuth::SaveUserConfig()
{
    std::ofstream file(USER_CONFIG_FILE);
    if (!file.is_open()) {
        return false;
    }

    // Write simple JSON
    file << "{\n";
    file << "  \"user_id\": \"" << m_userId << "\",\n";
    file << "  \"username\": \"" << m_username << "\"\n";
    file << "}\n";

    file.close();
    return true;
}

bool TwitchAuth::IsAuthenticated() const
{
    return !m_userId.empty();
}

bool TwitchAuth::StartAuthentication()
{
    std::cout << "[TwitchAuth] Starting authentication..." << std::endl;

    // Start local server to receive OAuth callback
    StartLocalServer();

    // Build OAuth URL with all required scopes
    std::stringstream ss;
    ss << "https://id.twitch.tv/oauth2/authorize"
        << "?client_id=" << m_clientId
        << "&redirect_uri=" << m_workerUrl << "/callback"
        << "&response_type=code"
        << "&scope=chat:read+chat:edit+bits:read"
        << "+channel:read:subscriptions+moderator:read:followers"
        << "+channel:read:redemptions+channel:manage:redemptions";

    std::string url = ss.str();
    std::cout << "[TwitchAuth] Opening browser to: " << url << std::endl;

    // Open browser
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    SetStatus(ConnectionStatus::Connecting, "Waiting for authorization...");
    return true;
}

void TwitchAuth::StartLocalServer()
{
    if (m_serverRunning) {
        return;
    }

    // Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET) {
        return;
    }

    // Allow reuse
    int opt = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Bind to localhost:3000
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    InetPtonA(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(CALLBACK_SERVER_PORT);

    if (bind(m_serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
        return;
    }

    if (listen(m_serverSocket, 1) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
        return;
    }

    // Start server thread
    m_serverRunning = true;
    m_serverThread = std::thread(&TwitchAuth::ServerLoop, this);
}

void TwitchAuth::StopLocalServer()
{
    if (!m_serverRunning) {
        return;
    }

    m_serverRunning = false;

    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
}

void TwitchAuth::ServerLoop()
{
    char buffer[4096];

    while (m_serverRunning) {
        // Accept connection with timeout
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_serverSocket, &readfds);

        timeval timeout = { 1, 0 }; // 1 second timeout
        int result = select(0, &readfds, nullptr, nullptr, &timeout);

        if (result <= 0) {
            continue; // Timeout or error, try again
        }

        SOCKET clientSocket = accept(m_serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        // Receive HTTP request
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string request(buffer);

            std::cout << "[TwitchAuth] Received HTTP request:" << std::endl;
            std::cout << request.substr(0, 200) << "..." << std::endl;

            // Handle OPTIONS request (CORS preflight)
            if (request.find("OPTIONS /auth/callback") == 0) {
                std::cout << "[TwitchAuth] Handling OPTIONS preflight request" << std::endl;
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                    "Access-Control-Allow-Headers: Content-Type\r\n"
                    "Access-Control-Max-Age: 86400\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n";
                send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
            // Check if it's a POST to /auth/callback
            else if (request.find("POST /auth/callback") == 0) {
                std::cout << "[TwitchAuth] POST /auth/callback received" << std::endl;

                // Find the JSON body
                size_t bodyStart = request.find("\r\n\r\n");
                if (bodyStart != std::string::npos) {
                    std::string body = request.substr(bodyStart + 4);
                    std::cout << "[TwitchAuth] Callback body: " << body << std::endl;

                    // Parse JSON for user_id and user_login
                    size_t userIdPos = body.find("\"user_id\"");
                    if (userIdPos != std::string::npos) {
                        size_t valueStart = body.find("\"", userIdPos + 10);
                        size_t valueEnd = body.find("\"", valueStart + 1);
                        if (valueStart != std::string::npos && valueEnd != std::string::npos) {
                            m_userId = body.substr(valueStart + 1, valueEnd - valueStart - 1);
                            std::cout << "[TwitchAuth] Parsed user_id: " << m_userId << std::endl;
                        }
                    }

                    size_t loginPos = body.find("\"user_login\"");
                    if (loginPos != std::string::npos) {
                        size_t valueStart = body.find("\"", loginPos + 13);
                        size_t valueEnd = body.find("\"", valueStart + 1);
                        if (valueStart != std::string::npos && valueEnd != std::string::npos) {
                            m_username = body.substr(valueStart + 1, valueEnd - valueStart - 1);
                            std::cout << "[TwitchAuth] Parsed username: " << m_username << std::endl;
                        }
                    }

                    // Save config
                    if (!m_userId.empty()) {
                        SaveUserConfig();
                        std::cout << "[TwitchAuth] Config saved, starting auto-connect..." << std::endl;

                        // Automatically connect to user's channel after authentication
                        std::thread([this]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Brief delay
                            std::cout << "[TwitchAuth] Auto-connecting to channel: " << m_username << std::endl;
                            Connect(m_username); // Connect to user's own channel
                            }).detach();
                    }
                }

                // Send response
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\n"
                    "{\"status\":\"success\"}";
                send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
        }

        closesocket(clientSocket);
    }
}

std::string TwitchAuth::GetAccessToken()
{
    if (m_userId.empty()) {
        return "";
    }

    // Call worker API to get valid token
    std::stringstream ss;
    ss << m_workerUrl << "/api/get-token?user_id=" << m_userId;

    std::string response = HttpGet(ss.str());

    if (response.empty()) {
        return "";
    }

    // Parse access_token from JSON response
    size_t tokenPos = response.find("\"access_token\"");
    if (tokenPos != std::string::npos) {
        size_t valueStart = response.find("\"", tokenPos + 15);
        size_t valueEnd = response.find("\"", valueStart + 1);
        if (valueStart != std::string::npos && valueEnd != std::string::npos) {
            return response.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    }

    return "";
}

void TwitchAuth::RevokeAuthentication()
{
    if (m_userId.empty()) {
        return;
    }

    // Call worker API to revoke
    std::stringstream ss;
    ss << m_workerUrl << "/api/revoke?user_id=" << m_userId;
    HttpGet(ss.str());

    // Clear local config
    m_userId.clear();
    m_username.clear();
    SaveUserConfig();
}

bool TwitchAuth::Connect(const std::string& channel)
{
    std::cout << "[TwitchAuth] Connect called for channel: " << channel << std::endl;

    if (m_status == ConnectionStatus::Connected) {
        std::cout << "[TwitchAuth] Already connected" << std::endl;
        return true;
    }

    if (!IsAuthenticated()) {
        std::cout << "[TwitchAuth] Not authenticated!" << std::endl;
        SetStatus(ConnectionStatus::Error, "Not authenticated. Please connect to Twitch first.");
        return false;
    }

    std::cout << "[TwitchAuth] Authenticated as user_id: " << m_userId << ", username: " << m_username << std::endl;

    // Get token from worker
    std::cout << "[TwitchAuth] Getting access token from worker..." << std::endl;
    std::string token = GetAccessToken();
    if (token.empty()) {
        std::cout << "[TwitchAuth] Failed to get access token!" << std::endl;
        SetStatus(ConnectionStatus::Error, "Failed to get access token");
        return false;
    }
    std::cout << "[TwitchAuth] Got access token (length: " << token.length() << ")" << std::endl;

    m_channel = channel;
    SetStatus(ConnectionStatus::Connecting, "Connecting to Twitch IRC...");

    if (!InitializeSocket()) {
        std::cout << "[TwitchAuth] Failed to create socket!" << std::endl;
        SetStatus(ConnectionStatus::Error, "Failed to create socket");
        return false;
    }
    std::cout << "[TwitchAuth] Socket created successfully" << std::endl;

    // Connect to Twitch IRC
    std::cout << "[TwitchAuth] Resolving irc.chat.twitch.tv:6667..." << std::endl;
    struct addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo("irc.chat.twitch.tv", "6667", &hints, &result) != 0) {
        std::cout << "[TwitchAuth] Failed to resolve Twitch IRC server!" << std::endl;
        CleanupSocket();
        SetStatus(ConnectionStatus::Error, "Failed to resolve Twitch IRC server");
        return false;
    }
    std::cout << "[TwitchAuth] DNS resolved successfully" << std::endl;

    std::cout << "[TwitchAuth] Connecting to IRC server..." << std::endl;
    if (connect(m_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        std::cout << "[TwitchAuth] Failed to connect to Twitch IRC! Error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        CleanupSocket();
        SetStatus(ConnectionStatus::Error, "Failed to connect to Twitch IRC");
        return false;
    }
    std::cout << "[TwitchAuth] Connected to IRC server!" << std::endl;

    freeaddrinfo(result);

    // Send authentication
    std::cout << "[TwitchAuth] Sending authentication..." << std::endl;
    std::stringstream ss;
    ss << "PASS oauth:" << token << "\r\n";
    ss << "NICK " << m_username << "\r\n";
    send(m_socket, ss.str().c_str(), (int)ss.str().length(), 0);

    // Request capabilities
    std::cout << "[TwitchAuth] Requesting capabilities..." << std::endl;
    std::string caps = "CAP REQ :twitch.tv/membership twitch.tv/tags twitch.tv/commands\r\n";
    send(m_socket, caps.c_str(), (int)caps.length(), 0);

    // Join channel
    std::cout << "[TwitchAuth] Joining channel #" << m_channel << "..." << std::endl;
    std::string join = "JOIN #" + m_channel + "\r\n";
    send(m_socket, join.c_str(), (int)join.length(), 0);

    // Start receive thread
    std::cout << "[TwitchAuth] Starting receive thread..." << std::endl;
    m_running = true;
    m_receiveThread = std::thread(&TwitchAuth::ReceiveLoop, this);

    SetStatus(ConnectionStatus::Connected, "Connected to #" + m_channel);
    std::cout << "[TwitchAuth] Connection complete!" << std::endl;
    return true;
}

void TwitchAuth::Disconnect()
{
    if (m_status == ConnectionStatus::Disconnected) {
        return;
    }

    m_running = false;

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    CleanupSocket();
    SetStatus(ConnectionStatus::Disconnected, "Disconnected from Twitch");
}

bool TwitchAuth::SendMessage(const std::string& message)
{
    if (m_status != ConnectionStatus::Connected) {
        return false;
    }

    std::string msg = "PRIVMSG #" + m_channel + " :" + message + "\r\n";
    int result = send(m_socket, msg.c_str(), (int)msg.length(), 0);
    return result != SOCKET_ERROR;
}

std::vector<ChatMessage> TwitchAuth::GetMessages() const
{
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    return m_messages;
}

void TwitchAuth::SetOnMessageCallback(std::function<void(const ChatMessage&)> callback)
{
    m_onMessage = callback;
}

void TwitchAuth::SetOnStatusCallback(std::function<void(ConnectionStatus, const std::string&)> callback)
{
    m_onStatus = callback;
}

void TwitchAuth::ReceiveLoop()
{
    std::cout << "[TwitchAuth] ReceiveLoop started" << std::endl;
    char buffer[4096];
    std::string incomplete;

    while (m_running) {
        int bytesReceived = recv(m_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived <= 0) {
            if (m_running) {
                std::cout << "[TwitchAuth] Connection lost in ReceiveLoop" << std::endl;
                SetStatus(ConnectionStatus::Error, "Connection lost");
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        incomplete += buffer;

        // Process complete lines
        size_t pos;
        while ((pos = incomplete.find("\r\n")) != std::string::npos) {
            std::string line = incomplete.substr(0, pos);
            incomplete = incomplete.substr(pos + 2);
            std::cout << "[IRC] " << line << std::endl;
            ProcessLine(line);
        }
    }
    std::cout << "[TwitchAuth] ReceiveLoop ended" << std::endl;
}


void TwitchAuth::ProcessLine(const std::string& line)
{
    // Handle PING
    if (line.find("PING") == 0) {
        std::string pong = "PONG :tmi.twitch.tv\r\n";
        send(m_socket, pong.c_str(), (int)pong.length(), 0);
        return;
    }

    // Handle PRIVMSG (chat messages)
    if (line.find("PRIVMSG") != std::string::npos) {
        ParseMessage(line);
    }
}

void TwitchAuth::ParseMessage(const std::string& line)
{
    // IRC format: :user!user@user.tmi.twitch.tv PRIVMSG #channel :message

    size_t userStart = line.find(':') + 1;
    size_t userEnd = line.find('!');

    size_t msgStart = line.find(':', userEnd);

    if (userStart == std::string::npos || userEnd == std::string::npos || msgStart == std::string::npos) {
        return;
    }

    ChatMessage msg;
    msg.username = line.substr(userStart, userEnd - userStart);
    msg.message = line.substr(msgStart + 1);
    msg.timestamp = GetCurrentTimestamp();

    // Add to messages list
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_messages.push_back(msg);

        // Keep only MAX_MESSAGES
        if (m_messages.size() > MAX_MESSAGES) {
            m_messages.erase(m_messages.begin());
        }
    }

    // Call callback
    if (m_onMessage) {
        m_onMessage(msg);
    }
}

void TwitchAuth::SetStatus(ConnectionStatus status, const std::string& message)
{
    m_status = status;
    m_statusMessage = message;

    if (m_onStatus) {
        m_onStatus(status, message);
    }
}

bool TwitchAuth::InitializeSocket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return m_socket != INVALID_SOCKET;
}

void TwitchAuth::CleanupSocket()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

std::string TwitchAuth::GetCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);

    std::stringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    return ss.str();
}

std::string TwitchAuth::HttpGet(const std::string& url)
{
    // Parse URL
    size_t schemeEnd = url.find("://");
    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);

    std::wstring host(url.begin() + hostStart, url.begin() + pathStart);
    std::wstring path(url.begin() + pathStart, url.end());

    HINTERNET hSession = WinHttpOpen(L"GobTalk/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    BOOL result = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    std::string response;
    if (result) {
        result = WinHttpReceiveResponse(hRequest, nullptr);
        if (result) {
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
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string TwitchAuth::HttpGetWithAuth(const std::string& url, const std::string& token)
{
    // Parse URL
    size_t schemeEnd = url.find("://");
    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);

    std::wstring host(url.begin() + hostStart, url.begin() + pathStart);
    std::wstring path(url.begin() + pathStart, url.end());

    HINTERNET hSession = WinHttpOpen(L"GobTalk/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Add authorization header
    std::wstring authHeader = L"Authorization: Bearer ";
    authHeader += std::wstring(token.begin(), token.end());
    authHeader += L"\r\nClient-Id: ";
    authHeader += std::wstring(m_clientId.begin(), m_clientId.end());
    authHeader += L"\r\n";

    BOOL result = WinHttpSendRequest(hRequest,
        authHeader.c_str(), (DWORD)authHeader.length(),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    std::string response;
    if (result) {
        result = WinHttpReceiveResponse(hRequest, nullptr);
        if (result) {
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
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}