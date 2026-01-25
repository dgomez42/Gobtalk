#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "TwitchMessageHandler.h"
#include "Twitchauth.h"
#include <iostream>
#include <sstream>
#include <algorithm>

TwitchMessageHandler::TwitchMessageHandler(DECtalkWrapper& dectalk)
    : m_dectalk(dectalk)
    , m_twitchAuth(nullptr)
    , m_ttsMode(TTSMode::Queue)
    , m_running(false)
{
    std::cout << "[MessageHandler] Created" << std::endl;
}

TwitchMessageHandler::~TwitchMessageHandler()
{
    Stop();
}

void TwitchMessageHandler::SetTwitchAuth(TwitchAuth* auth)
{
    m_twitchAuth = auth;

    // Set up callback for raw IRC messages so we can parse tags
    // Note: We'll need to modify TwitchAuth to provide raw messages
}

void TwitchMessageHandler::Start()
{
    if (m_running) return;

    std::cout << "[MessageHandler] Starting..." << std::endl;

    // Join old thread if it exists
    if (m_processThread.joinable()) {
        std::cout << "[MessageHandler] Joining old process thread..." << std::endl;
        m_running = false;
        m_processThread.join();
    }

    m_running = true;
    m_processThread = std::thread(&TwitchMessageHandler::ProcessLoop, this);
}

void TwitchMessageHandler::Stop()
{
    if (!m_running) return;

    std::cout << "[MessageHandler] Stopping..." << std::endl;
    m_running = false;

    if (m_processThread.joinable()) {
        m_processThread.join();
    }
}

void TwitchMessageHandler::OnMessage(const ChatMessage& msg)
{
    std::cout << "[MessageHandler] Received message from " << msg.username << ": " << msg.message << std::endl;

    // For now, create a basic extended message
    // Later we'll parse this from raw IRC with tags
    ExtendedChatMessage extMsg;
    extMsg.username = msg.username;
    extMsg.displayName = msg.username;
    extMsg.message = msg.message;
    extMsg.timestamp = msg.timestamp;
    extMsg.isModerator = false;
    extMsg.isSubscriber = false;
    extMsg.isVIP = false;
    extMsg.isTurbo = false;
    extMsg.isBroadcaster = false;
    extMsg.subscriberMonths = 0;
    extMsg.bits = 0;

    // Add to recent messages
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_recentMessages.push_back(extMsg);
        if (m_recentMessages.size() > MAX_RECENT_MESSAGES) {
            m_recentMessages.erase(m_recentMessages.begin());
        }
    }

    ProcessChatMessage(extMsg);
}

void TwitchMessageHandler::OnRawIRCMessage(const std::string& rawMessage)
{
    std::cout << "[MessageHandler] Raw IRC: " << rawMessage << std::endl;

    // Only process certain IRC message types
    // Ignore: PING, JOIN, PART, 001, 002, 003, 004, 353, 366, 372, 375, 376, CAP, etc.

    // Check for PRIVMSG (regular chat messages)
    if (rawMessage.find("PRIVMSG") != std::string::npos) {
        ExtendedChatMessage msg = ParseIRCMessage(rawMessage);

        // Validate message has content
        if (msg.username.empty() || msg.message.empty()) {
            std::cout << "[MessageHandler] Skipping empty message" << std::endl;
            return;
        }

        // Add to recent messages
        {
            std::lock_guard<std::mutex> lock(m_messagesMutex);
            m_recentMessages.push_back(msg);
            if (m_recentMessages.size() > MAX_RECENT_MESSAGES) {
                m_recentMessages.erase(m_recentMessages.begin());
            }
        }

        // Process bits if present
        if (msg.bits > 0) {
            ProcessBits(msg);
        }
        // Process channel point redeems
        else if (rawMessage.find("custom-reward-id") != std::string::npos) {
            ProcessChannelPointRedeem(rawMessage);
        }
        // Regular chat message
        else {
            ProcessChatMessage(msg);
        }
    }
    // Check for USERNOTICE (subscriptions, resubs, gifted subs)
    else if (rawMessage.find("USERNOTICE") != std::string::npos) {
        ProcessSubscription(rawMessage);
    }
    // Ignore all other IRC messages (PING, JOIN, PART, etc.)
}

void TwitchMessageHandler::OnNewFollower(const std::string& username)
{
    std::cout << "[MessageHandler] New follower: " << username << std::endl;
    ProcessFollower(username);
}

ExtendedChatMessage TwitchMessageHandler::ParseIRCMessage(const std::string& rawMessage)
{
    ExtendedChatMessage msg;
    msg.rawTags = rawMessage;

    // IRC format with tags: @badges=... :user!user@user.tmi.twitch.tv PRIVMSG #channel :message
    // IRC format without tags: :user!user@user.tmi.twitch.tv PRIVMSG #channel :message

    std::string parseFrom = rawMessage;
    std::map<std::string, std::string> tags;

    // Check if message has tags (starts with @)
    if (rawMessage[0] == '@') {
        size_t tagEnd = rawMessage.find(" :");
        if (tagEnd != std::string::npos) {
            std::string tagsSection = rawMessage.substr(1, tagEnd - 1); // Skip initial @
            tags = ParseIRCTags(tagsSection);
            parseFrom = rawMessage.substr(tagEnd + 1); // Parse from after tags
        }
    }

    // Extract username - find :user!user@user.tmi.twitch.tv
    size_t userStart = parseFrom.find(':');
    if (userStart == std::string::npos) return msg;

    userStart++; // Skip the :
    size_t userEnd = parseFrom.find('!', userStart);
    if (userEnd != std::string::npos) {
        msg.username = parseFrom.substr(userStart, userEnd - userStart);
    }

    // Extract message - find the message content after PRIVMSG #channel :
    size_t privmsgPos = parseFrom.find("PRIVMSG");
    if (privmsgPos != std::string::npos) {
        size_t msgStart = parseFrom.find(':', privmsgPos);
        if (msgStart != std::string::npos) {
            msg.message = parseFrom.substr(msgStart + 1);

            // Remove trailing \r\n if present
            if (!msg.message.empty() && msg.message.back() == '\n') {
                msg.message.pop_back();
            }
            if (!msg.message.empty() && msg.message.back() == '\r') {
                msg.message.pop_back();
            }
        }
    }

    // Parse tags if they exist
    msg.displayName = tags.count("display-name") ? tags["display-name"] : msg.username;
    msg.userId = tags.count("user-id") ? tags["user-id"] : "";

    // Parse badges
    if (tags.count("badges")) {
        std::string badges = tags["badges"];
        msg.isModerator = (badges.find("moderator") != std::string::npos);
        msg.isSubscriber = (badges.find("subscriber") != std::string::npos);
        msg.isVIP = (badges.find("vip") != std::string::npos);
        msg.isTurbo = (badges.find("turbo") != std::string::npos);
        msg.isBroadcaster = (badges.find("broadcaster") != std::string::npos);

        // Parse subscriber months from badge info
        if (msg.isSubscriber && tags.count("badge-info")) {
            std::string badgeInfo = tags["badge-info"];
            size_t subPos = badgeInfo.find("subscriber/");
            if (subPos != std::string::npos) {
                size_t monthStart = subPos + 11; // Length of "subscriber/"
                size_t monthEnd = badgeInfo.find_first_not_of("0123456789", monthStart);
                std::string monthsStr = badgeInfo.substr(monthStart, monthEnd - monthStart);
                if (!monthsStr.empty()) {
                    msg.subscriberMonths = std::stoi(monthsStr);
                }
            }
        }
    }

    // Parse bits
    if (tags.count("bits") && !tags["bits"].empty()) {
        msg.bits = std::stoi(tags["bits"]);
    }

    return msg;
}

std::map<std::string, std::string> TwitchMessageHandler::ParseIRCTags(const std::string& tags)
{
    std::map<std::string, std::string> result;

    std::istringstream stream(tags);
    std::string tag;

    while (std::getline(stream, tag, ';')) {
        size_t equalPos = tag.find('=');
        if (equalPos != std::string::npos) {
            std::string key = tag.substr(0, equalPos);
            std::string value = tag.substr(equalPos + 1);
            result[key] = value;
        }
    }

    return result;
}

bool TwitchMessageHandler::ShouldReadMessage(const ExtendedChatMessage& msg) const
{
    // If role filter is set to Everyone, always read
    if (roleFilterMode == RoleFilterMode::Everyone) {
        return true;
    }

    // Custom mode - check if user has any allowed role
    // Broadcaster always passes (they own the channel)
    if (msg.isBroadcaster) {
        return true;
    }

    // Check each role
    if (msg.isModerator && allowMods) {
        return true;
    }

    if (msg.isVIP && allowVIPs) {
        return true;
    }

    if (msg.isSubscriber && allowSubscribers) {
        return true;
    }

    if (msg.isTurbo && allowTurbo) {
        return true;
    }

    // TODO: Follower detection requires EventSub or API call
    // For now, if allowFollowers is true and they don't have other roles,
    // we'll allow them (assuming everyone is at least a follower or viewer)
    if (allowFollowers) {
        return true;
    }

    // No matching roles - don't read
    return false;
}

void TwitchMessageHandler::ProcessChatMessage(const ExtendedChatMessage& msg)
{
    if (!enableChatMessages) return;

    // Check if this user's role is allowed to have messages read
    if (!ShouldReadMessage(msg)) {
        std::cout << "[MessageHandler] Skipping message from " << msg.displayName
            << " (role not allowed)" << std::endl;
        return;
    }

    // Check if !say command is REQUIRED
    if (enableSayCommand) {
        // !say command is REQUIRED - only process messages starting with !say
        if (msg.message.find("!say ") == 0) {
            ProcessSayCommand(msg);
        }
        else {
            std::cout << "[MessageHandler] Skipping message from " << msg.displayName
                << " (!say command required)" << std::endl;
        }
        return;
    }

    // !say command is OFF - read all regular chat messages normally
    std::string ttsText;
    if (readUsername) {
        ttsText = msg.displayName + " says: " + msg.message;
    }
    else {
        ttsText = msg.message;
    }

    std::cout << "[MessageHandler] Queuing TTS: " << ttsText << std::endl;
    QueueTTS(ttsText);
}

void TwitchMessageHandler::ProcessSayCommand(const ExtendedChatMessage& msg)
{
    // Extract text after "!say "
    std::string textToSay = msg.message.substr(5);

    // Respect readUsername toggle
    std::string ttsText;
    if (readUsername) {
        ttsText = msg.displayName + " says: " + textToSay;
    }
    else {
        ttsText = textToSay;
    }

    std::cout << "[MessageHandler] !say command from " << msg.displayName << ": " << ttsText << std::endl;
    QueueTTS(ttsText);
}

std::string TwitchMessageHandler::ReplaceVariables(const std::string& message, const std::string& username, int months) const
{
    std::string result = message;

    // Replace {username} with actual username
    size_t pos = 0;
    while ((pos = result.find("{username}", pos)) != std::string::npos) {
        result.replace(pos, 10, username); // 10 is length of "{username}"
        pos += username.length();
    }

    // Replace {months} with actual month count
    if (months > 0) {
        pos = 0;
        std::string monthsStr = std::to_string(months);
        while ((pos = result.find("{months}", pos)) != std::string::npos) {
            result.replace(pos, 8, monthsStr); // 8 is length of "{months}"
            pos += monthsStr.length();
        }
    }

    return result;
}

void TwitchMessageHandler::ProcessSubscription(const std::string& rawMessage)
{
    if (!enableSubscriberAnnouncements) return;

    std::cout << "[MessageHandler] Processing subscription: " << rawMessage << std::endl;

    // Parse subscription type from msg-id tag
    size_t msgIdPos = rawMessage.find("msg-id=");
    if (msgIdPos == std::string::npos) return;

    std::string msgId = rawMessage.substr(msgIdPos + 7, 20); // Get portion after msg-id=
    size_t msgIdEnd = msgId.find(';');
    if (msgIdEnd != std::string::npos) {
        msgId = msgId.substr(0, msgIdEnd);
    }

    // Parse username from display-name tag
    std::string username = "Someone";
    size_t displayPos = rawMessage.find("display-name=");
    if (displayPos != std::string::npos) {
        size_t nameStart = displayPos + 13;
        size_t nameEnd = rawMessage.find(';', nameStart);
        username = rawMessage.substr(nameStart, nameEnd - nameStart);
    }

    std::string announcement;

    if (msgId == "sub" || msgId == "resub") {
        // Get month count for resubs
        int months = 1;
        size_t monthsPos = rawMessage.find("msg-param-cumulative-months=");
        if (monthsPos != std::string::npos) {
            size_t monthStart = monthsPos + 28;
            size_t monthEnd = rawMessage.find(';', monthStart);
            std::string monthStr = rawMessage.substr(monthStart, monthEnd - monthStart);
            if (!monthStr.empty()) {
                months = std::stoi(monthStr);
            }
        }

        if (msgId == "sub") {
            // Use custom subscriber message
            announcement = ReplaceVariables(subscriberMessage, username);
        }
        else {
            // Use custom resubscriber message with months
            announcement = ReplaceVariables(resubscriberMessage, username, months);
        }
    }
    else if (msgId == "subgift") {
        // Sub gifts use the subscriber message
        announcement = ReplaceVariables(subscriberMessage, username);
    }

    if (!announcement.empty()) {
        std::cout << "[MessageHandler] Sub announcement: " << announcement << std::endl;
        QueueTTS(announcement);
    }
}

void TwitchMessageHandler::ProcessBits(const ExtendedChatMessage& msg)
{
    if (!enableBitsAnnouncements) return;
    if (msg.bits < minBitsToAnnounce) return;

    std::string announcement = msg.displayName + " cheered " + std::to_string(msg.bits) + " bits!";
    std::cout << "[MessageHandler] Bits announcement: " << announcement << std::endl;
    QueueTTS(announcement);
}

void TwitchMessageHandler::ProcessChannelPointRedeem(const std::string& rawMessage)
{
    if (!enableChannelPointRedeems) return;

    std::cout << "[MessageHandler] Channel point redeem: " << rawMessage << std::endl;

    // Parse username
    std::string username = "Someone";
    size_t displayPos = rawMessage.find("display-name=");
    if (displayPos != std::string::npos) {
        size_t nameStart = displayPos + 13;
        size_t nameEnd = rawMessage.find(';', nameStart);
        username = rawMessage.substr(nameStart, nameEnd - nameStart);
    }

    // Parse user input (if any)
    std::string userInput = "";
    size_t msgStart = rawMessage.find(" USERNOTICE ");
    if (msgStart != std::string::npos) {
        size_t colonPos = rawMessage.find(':', msgStart);
        if (colonPos != std::string::npos && colonPos + 1 < rawMessage.length()) {
            userInput = rawMessage.substr(colonPos + 1);
            // Remove trailing whitespace/newlines
            while (!userInput.empty() && (userInput.back() == '\r' || userInput.back() == '\n')) {
                userInput.pop_back();
            }
        }
    }

    // Only announce if there's user input (like EventSub version)
    if (!userInput.empty()) {
        std::string announcement;

        // Respect "Read Username" toggle
        if (readUsername) {
            announcement = username + " says: " + userInput;
        }
        else {
            announcement = userInput;
        }

        QueueTTS(announcement);
    }
}

void TwitchMessageHandler::ProcessFollower(const std::string& username)
{
    if (!enableFollowerAnnouncements) return;

    std::cout << "[MessageHandler] Processing follower: " << username << std::endl;

    // Use custom follower message
    std::string announcement = ReplaceVariables(followerMessage, username);

    std::cout << "[MessageHandler] Follower announcement: " << announcement << std::endl;
    QueueTTS(announcement);
}

void TwitchMessageHandler::QueueTTS(const std::string& text)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    switch (m_ttsMode) {
    case TTSMode::Queue:
        m_queueMode.push(text);
        break;

    case TTSMode::KeepUp:
        // Clear old messages and add new one
        while (!m_keepUpMode.empty()) {
            m_keepUpMode.pop();
        }
        m_keepUpMode.push(text);
        break;

    case TTSMode::Goblin:
        // Speak immediately, don't queue
        SpeakImmediate(text);
        break;
    }
}

void TwitchMessageHandler::SpeakImmediate(const std::string& text)
{
    // For Goblin mode - speak without waiting
    m_dectalk.Speak(text);
}

void TwitchMessageHandler::ProcessLoop()
{
    std::cout << "[MessageHandler] Process loop started" << std::endl;

    while (m_running) {
        std::string textToSpeak;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);

            if (m_ttsMode == TTSMode::Queue && !m_queueMode.empty()) {
                textToSpeak = m_queueMode.front();
                m_queueMode.pop();
            }
            else if (m_ttsMode == TTSMode::KeepUp && !m_keepUpMode.empty()) {
                textToSpeak = m_keepUpMode.front();
                m_keepUpMode.pop();
            }
        }

        if (!textToSpeak.empty()) {
            std::cout << "[MessageHandler] Speaking: " << textToSpeak << std::endl;
            m_dectalk.Speak(textToSpeak);
        }

        // Small delay to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[MessageHandler] Process loop ended" << std::endl;
}

std::vector<ExtendedChatMessage> TwitchMessageHandler::GetRecentMessages(int count) const
{
    std::lock_guard<std::mutex> lock(m_messagesMutex);

    int start = (std::max)(0, (int)m_recentMessages.size() - count);
    return std::vector<ExtendedChatMessage>(
        m_recentMessages.begin() + start,
        m_recentMessages.end()
    );
}