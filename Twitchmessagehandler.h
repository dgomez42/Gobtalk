#pragma once
#include "DECtalkWrapper.h"
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <map>

// Forward declarations
class TwitchAuth;
struct ChatMessage;

// Extended message info with user roles and metadata
struct ExtendedChatMessage {
    std::string username;
    std::string displayName;
    std::string message;
    std::string userId;
    std::string timestamp;

    // User roles/badges
    bool isModerator;
    bool isSubscriber;
    bool isVIP;
    bool isTurbo;
    bool isBroadcaster;
    int subscriberMonths; // 0 if not subscribed

    // Message metadata
    int bits; // Number of bits in message (0 if none)
    std::string rawTags; // Raw IRC tags for debugging
};

// TTS modes
enum class TTSMode {
    Queue,    // Read messages one at a time in order
    KeepUp,   // Skip old messages, always read latest
    Goblin    // Read all messages simultaneously (chaos mode)
};

class TwitchMessageHandler {
public:
    TwitchMessageHandler(DECtalkWrapper& dectalk);
    ~TwitchMessageHandler();

    // Setup
    void SetTwitchAuth(TwitchAuth* auth);
    void Start();
    void Stop();
    void ClearQueue(); // Clear all pending messages

    // TTS Mode
    void SetTTSMode(TTSMode mode) { m_ttsMode = mode; }
    TTSMode GetTTSMode() const { return m_ttsMode; }

    // Feature toggles (will be controlled by UI)
    bool enableChatMessages = true;
    bool enableSayCommand = true;
    bool readUsername = true;  // Include "Username says:" prefix
    bool enableSubscriberAnnouncements = true;
    bool enableFollowerAnnouncements = true;
    bool enableBitsAnnouncements = true;
    bool enableChannelPointRedeems = true;

    // Role filtering
    enum class RoleFilterMode {
        Everyone,  // Read messages from everyone
        Custom     // Only read from specific roles
    };
    RoleFilterMode roleFilterMode = RoleFilterMode::Everyone;

    // Custom role toggles (only used when roleFilterMode == Custom)
    bool allowFollowers = true;
    bool allowSubscribers = true;
    bool allowTurbo = true;
    bool allowMods = true;
    bool allowVIPs = true;

    // Thresholds
    int minBitsToAnnounce = 1;
    int minDonationToAnnounce = 1; // in dollars

    // Custom announcement messages (use {username} as placeholder)
    std::string followerMessage = "{username} just followed!";
    std::string subscriberMessage = "{username} just subscribed!";
    std::string resubscriberMessage = "{username} resubscribed for {months} months!";

    // Message callback from TwitchAuth
    void OnMessage(const ChatMessage& msg);
    void OnRawIRCMessage(const std::string& rawMessage);
    void OnNewFollower(const std::string& username);

    // Get recent messages for display
    std::vector<ExtendedChatMessage> GetRecentMessages(int count = 50) const;

private:
    DECtalkWrapper& m_dectalk;
    TwitchAuth* m_twitchAuth;

    TTSMode m_ttsMode;
    bool m_running;

    // Message queues
    std::queue<std::string> m_queueMode;
    std::queue<std::string> m_keepUpMode;
    std::mutex m_queueMutex;

    // Processing thread
    std::thread m_processThread;
    void ProcessLoop();

    // Recent messages for display
    std::vector<ExtendedChatMessage> m_recentMessages;
    mutable std::mutex m_messagesMutex;
    static constexpr size_t MAX_RECENT_MESSAGES = 100;

    // IRC parsing
    ExtendedChatMessage ParseIRCMessage(const std::string& rawMessage);
    std::map<std::string, std::string> ParseIRCTags(const std::string& tags);

    // Role filtering
    bool ShouldReadMessage(const ExtendedChatMessage& msg) const;

    // Message processing
    void ProcessChatMessage(const ExtendedChatMessage& msg);
    void ProcessSayCommand(const ExtendedChatMessage& msg);
    void ProcessSubscription(const std::string& rawMessage);
    void ProcessBits(const ExtendedChatMessage& msg);
    void ProcessChannelPointRedeem(const std::string& rawMessage);
    void ProcessFollower(const std::string& username);

    // Custom message helpers
    std::string ReplaceVariables(const std::string& message, const std::string& username, int months = 0) const;

    // TTS helpers
    void QueueTTS(const std::string& text);
    void SpeakImmediate(const std::string& text); // For Goblin mode
};