#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "DECtalkWrapper.h"
#include "Twitchauth.h"
#include "TwitchMessageHandler.h"
#include "TwitchEventSub.h"
#include "GoblinMode.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <tchar.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <memory>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ws2_32.lib")
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DECTalk instance
DECtalkWrapper dectalk;

// GoblinMode instance (for CHAOS!)
GoblinMode goblinMode;

// Twitch instance
TwitchAuth twitchAuth;

// Twitch EventSub instance
TwitchEventSub eventSub;

// Message handler instance (created after DECtalk initialization)
std::unique_ptr<TwitchMessageHandler> messageHandler;

// Icon textures for connection status
ID3D11ShaderResourceView* g_iconNoNetwork = nullptr;
ID3D11ShaderResourceView* g_iconConnected = nullptr;

// UI State
static int selectedVoice = 0;
static const char* voiceNames[] = { "Paul", "Betty", "Harry", "Frank", "Dennis", "Kit", "Ursula", "Rita", "Wendy" };
static int volume = 100;
static char textBuffer[1024] = "Hello, I am DECtalk.";
static char channelPointRewardName[256] = "";  // Filter for channel point redemptions
static int selectedQueueMode = 1;  // 0 = Queue, 1 = Pace (default), 2 = Goblin
static const char* queueModeNames[] = { "Queue", "Pace", "Goblin" };

// Filter state
static std::vector<std::string> bannedUsernames;
static std::vector<std::string> bannedWords;
static char bannedUsernamesText[4096] = "";  // Multiline text buffer for usernames
static char bannedWordsText[4096] = "";      // Multiline text buffer for words

// Window state
static bool isMaximized = false;
static RECT restoreRect = {};
static bool showAboutDialog = false;

// Menu state
static int openMenuIndex = -1;  // -1 = none, 0 = File, 1 = Edit, 2 = Help
static bool justOpenedMenu = false;  // Prevent closing menu on same frame it was opened

// Helper functions to sync filter text buffers with vectors
void UpdateBannedUsernamesFromText() {
    bannedUsernames.clear();
    std::string text(bannedUsernamesText);
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            bannedUsernames.push_back(line);
        }
    }
}

void UpdateBannedWordsFromText() {
    bannedWords.clear();
    std::string text(bannedWordsText);
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            bannedWords.push_back(line);
        }
    }
}

void UpdateTextFromBannedUsernames() {
    bannedUsernamesText[0] = '\0';
    for (size_t i = 0; i < bannedUsernames.size(); i++) {
        strcat_s(bannedUsernamesText, sizeof(bannedUsernamesText), bannedUsernames[i].c_str());
        if (i < bannedUsernames.size() - 1) {
            strcat_s(bannedUsernamesText, sizeof(bannedUsernamesText), "\n");
        }
    }
}

void UpdateTextFromBannedWords() {
    bannedWordsText[0] = '\0';
    for (size_t i = 0; i < bannedWords.size(); i++) {
        strcat_s(bannedWordsText, sizeof(bannedWordsText), bannedWords[i].c_str());
        if (i < bannedWords.size() - 1) {
            strcat_s(bannedWordsText, sizeof(bannedWordsText), "\n");
        }
    }
}

// Settings file functions
void SaveSettings(const char* filename = "gobtalk_settings.ini") {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Settings] Failed to save settings to " << filename << std::endl;
        return;
    }

    file << "[Voice]" << std::endl;
    file << "SelectedVoice=" << selectedVoice << std::endl;
    file << "Volume=" << volume << std::endl;
    file << "QueueMode=" << selectedQueueMode << std::endl;

    file << "\n[Options]" << std::endl;
    if (messageHandler) {
        file << "EnableChatMessages=" << messageHandler->enableChatMessages << std::endl;
        file << "EnableSayCommand=" << messageHandler->enableSayCommand << std::endl;
        file << "ReadUsername=" << messageHandler->readUsername << std::endl;
        file << "RoleFilterMode=" << (int)messageHandler->roleFilterMode << std::endl;
        file << "AllowFollowers=" << messageHandler->allowFollowers << std::endl;
        file << "AllowSubscribers=" << messageHandler->allowSubscribers << std::endl;
        file << "AllowTurbo=" << messageHandler->allowTurbo << std::endl;
        file << "AllowMods=" << messageHandler->allowMods << std::endl;
        file << "AllowVIPs=" << messageHandler->allowVIPs << std::endl;
        file << "EnableSubscriberAnnouncements=" << messageHandler->enableSubscriberAnnouncements << std::endl;
        file << "EnableFollowerAnnouncements=" << messageHandler->enableFollowerAnnouncements << std::endl;
        file << "EnableBitsAnnouncements=" << messageHandler->enableBitsAnnouncements << std::endl;
        file << "EnableChannelPointRedeems=" << messageHandler->enableChannelPointRedeems << std::endl;
    }

    file << "\n[Filters]" << std::endl;
    file << "BannedUsernamesCount=" << bannedUsernames.size() << std::endl;
    for (size_t i = 0; i < bannedUsernames.size(); i++) {
        file << "BannedUser" << i << "=" << bannedUsernames[i] << std::endl;
    }
    file << "BannedWordsCount=" << bannedWords.size() << std::endl;
    for (size_t i = 0; i < bannedWords.size(); i++) {
        file << "BannedWord" << i << "=" << bannedWords[i] << std::endl;
    }

    file.close();
    std::cout << "[Settings] Settings saved to " << filename << std::endl;
}

void LoadSettings(const char* filename = "gobtalk_settings.ini") {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[Settings] No saved settings found, using defaults" << std::endl;
        return;
    }

    std::string line;
    std::string section;

    bannedUsernames.clear();
    bannedWords.clear();

    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        // Check for section headers
        if (line[0] == '[') {
            section = line;
            continue;
        }

        // Parse key=value
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Voice settings
        if (key == "SelectedVoice") {
            selectedVoice = std::stoi(value);
            dectalk.SetVoice(selectedVoice);  // Apply to DECtalk
        }
        else if (key == "Volume") {
            volume = std::stoi(value);
            dectalk.SetVolume(volume);
        }
        else if (key == "QueueMode") {
            selectedQueueMode = std::stoi(value);
            TTSMode mode;
            switch (selectedQueueMode) {
            case 0: mode = TTSMode::Queue; break;
            case 1: mode = TTSMode::KeepUp; break;
            case 2: mode = TTSMode::Goblin; break;
            default: mode = TTSMode::KeepUp; break;
            }
            if (messageHandler) messageHandler->SetTTSMode(mode);
        }
        // Options
        else if (messageHandler) {
            if (key == "EnableChatMessages") messageHandler->enableChatMessages = (value == "1");
            else if (key == "EnableSayCommand") messageHandler->enableSayCommand = (value == "1");
            else if (key == "ReadUsername") messageHandler->readUsername = (value == "1");
            else if (key == "RoleFilterMode") messageHandler->roleFilterMode = (TwitchMessageHandler::RoleFilterMode)std::stoi(value);
            else if (key == "AllowFollowers") messageHandler->allowFollowers = (value == "1");
            else if (key == "AllowSubscribers") messageHandler->allowSubscribers = (value == "1");
            else if (key == "AllowTurbo") messageHandler->allowTurbo = (value == "1");
            else if (key == "AllowMods") messageHandler->allowMods = (value == "1");
            else if (key == "AllowVIPs") messageHandler->allowVIPs = (value == "1");
            else if (key == "EnableSubscriberAnnouncements") messageHandler->enableSubscriberAnnouncements = (value == "1");
            else if (key == "EnableFollowerAnnouncements") messageHandler->enableFollowerAnnouncements = (value == "1");
            else if (key == "EnableBitsAnnouncements") messageHandler->enableBitsAnnouncements = (value == "1");
            else if (key == "EnableChannelPointRedeems") messageHandler->enableChannelPointRedeems = (value == "1");
        }
        // Filters
        else if (key.find("BannedUser") == 0 && key != "BannedUsernamesCount") {
            bannedUsernames.push_back(value);
        }
        else if (key.find("BannedWord") == 0 && key != "BannedWordsCount") {
            bannedWords.push_back(value);
        }
    }

    file.close();

    // Update text buffers from loaded vectors
    UpdateTextFromBannedUsernames();
    UpdateTextFromBannedWords();

    // Sync settings to GoblinMode
    goblinMode.SetVoice(selectedVoice);
    goblinMode.SetVolume(volume);

    std::cout << "[Settings] Settings loaded from " << filename << std::endl;
}

// Win98 Color Scheme
void ApplyWin98Style()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Classic Win98 colors
    colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Pure black text
    colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f); // Win98 gray
    colors[ImGuiCol_ChildBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White background for dropdowns/tooltips (Win98)
    colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_BorderShadow] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.5f, 1.0f); // Win98 blue
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.5f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.5f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    // Dropdown/Menu colors (Windows 98 style)
    colors[ImGuiCol_Header] = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);  // No highlight for selected item (transparent)
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.063f, 0.518f, 0.816f, 1.0f);  // Light blue matching title bar (RGB 16, 132, 208)
    colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.0f, 0.5f, 1.0f);   // Dark blue when clicked (Win98)

    colors[ImGuiCol_Separator] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

    // Win98 style settings
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 0.0f;     // We draw custom window border
    style.ChildBorderSize = 0.0f;      // We draw custom sunken borders
    style.FrameBorderSize = 1.0f;
    style.FramePadding = ImVec2(4.0f, 3.0f);  // Default for sliders, dropdowns, etc.
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ScrollbarSize = 16.0f;
}

// Helper macros for custom checkbox padding
#define BEGIN_CHECKBOX_STYLE() ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f))
#define END_CHECKBOX_STYLE() ImGui::PopStyleVar()

// Global tooltip padding setting - change this ONE value to adjust ALL tooltip sizes
static const ImVec2 TOOLTIP_PADDING = ImVec2(16.0f, 16.0f);
// Global tooltip font scale - 1.0 = normal, 1.2 = 20% larger, 0.8 = 20% smaller
static const float TOOLTIP_FONT_SCALE = 1.0f;
// Optional: Set to a specific ImFont* to use a different font for tooltips (nullptr = use default)
static ImFont* TOOLTIP_FONT = nullptr;

// Macro to start a custom tooltip with proper padding
#define BEGIN_TOOLTIP() \
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, TOOLTIP_PADDING); \
    ImGui::BeginTooltip(); \
    if (TOOLTIP_FONT != nullptr) ImGui::PushFont(TOOLTIP_FONT); \
    ImGui::SetWindowFontScale(TOOLTIP_FONT_SCALE); \
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

#define END_TOOLTIP() \
    ImGui::PopTextWrapPos(); \
    if (TOOLTIP_FONT != nullptr) ImGui::PopFont(); \
    ImGui::EndTooltip(); \
    ImGui::PopStyleVar();

// Helper function for simple one-line tooltips
inline void ShowTooltip(const char* text) {
    // IsItemHovered() without flags won't trigger on disabled items by default
    // This is the correct way to handle tooltips with disabled widgets
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        BEGIN_TOOLTIP()
            ImGui::TextUnformatted(text);
        END_TOOLTIP()
    }
}

// Draw Win98-style beveled border (inset or outset)
void DrawWin98Bevel(ImDrawList* draw_list, ImVec2 min, ImVec2 max, bool sunken = false, int thickness = 2)
{
    ImU32 lightOuter = IM_COL32(255, 255, 255, 255);   // Bright white
    ImU32 lightInner = IM_COL32(223, 223, 223, 255);   // Light gray
    ImU32 darkInner = IM_COL32(128, 128, 128, 255);    // Dark gray
    ImU32 darkOuter = IM_COL32(0, 0, 0, 255);          // Black

    if (sunken) {
        // SUNKEN/INSET (content areas, text boxes)
        // Outer: dark on top-left, light on bottom-right
        draw_list->AddLine(ImVec2(min.x, max.y - 1), min, darkOuter, 1.0f);  // Left
        draw_list->AddLine(min, ImVec2(max.x, min.y), darkOuter, 1.0f);      // Top
        draw_list->AddLine(ImVec2(max.x - 1, min.y), ImVec2(max.x - 1, max.y), lightOuter, 1.0f);  // Right
        draw_list->AddLine(ImVec2(max.x - 1, max.y - 1), ImVec2(min.x, max.y - 1), lightOuter, 1.0f);  // Bottom

        if (thickness >= 2) {
            // Inner: dark gray on top-left, light gray on bottom-right
            draw_list->AddLine(ImVec2(min.x + 1, max.y - 2), ImVec2(min.x + 1, min.y + 1), darkInner, 1.0f);  // Left
            draw_list->AddLine(ImVec2(min.x + 1, min.y + 1), ImVec2(max.x - 1, min.y + 1), darkInner, 1.0f);  // Top
            draw_list->AddLine(ImVec2(max.x - 2, min.y + 1), ImVec2(max.x - 2, max.y - 1), lightInner, 1.0f);  // Right
            draw_list->AddLine(ImVec2(max.x - 2, max.y - 2), ImVec2(min.x + 1, max.y - 2), lightInner, 1.0f);  // Bottom
        }
    }
    else {
        // RAISED/OUTSET (window border, buttons, toolbars)
        // Outer: light on top-left, dark on bottom-right
        draw_list->AddLine(ImVec2(min.x, max.y - 1), min, lightOuter, 1.0f);  // Left
        draw_list->AddLine(min, ImVec2(max.x, min.y), lightOuter, 1.0f);      // Top
        draw_list->AddLine(ImVec2(max.x - 1, min.y), ImVec2(max.x - 1, max.y), darkOuter, 1.0f);  // Right
        draw_list->AddLine(ImVec2(max.x - 1, max.y - 1), ImVec2(min.x, max.y - 1), darkOuter, 1.0f);  // Bottom

        if (thickness >= 2) {
            // Inner: light gray on top-left, dark gray on bottom-right
            draw_list->AddLine(ImVec2(min.x + 1, max.y - 2), ImVec2(min.x + 1, min.y + 1), lightInner, 1.0f);  // Left
            draw_list->AddLine(ImVec2(min.x + 1, min.y + 1), ImVec2(max.x - 1, min.y + 1), lightInner, 1.0f);  // Top
            draw_list->AddLine(ImVec2(max.x - 2, min.y + 1), ImVec2(max.x - 2, max.y - 1), darkInner, 1.0f);  // Right
            draw_list->AddLine(ImVec2(max.x - 2, max.y - 2), ImVec2(min.x + 1, max.y - 2), darkInner, 1.0f);  // Bottom
        }
    }
}

// Draw Win98 style 3D border
void DrawWin98Border(ImDrawList* draw_list, ImVec2 min, ImVec2 max, bool pressed = false)
{
    ImU32 light = IM_COL32(255, 255, 255, 255);
    ImU32 dark = IM_COL32(64, 64, 64, 255);
    ImU32 shadow = IM_COL32(128, 128, 128, 255);

    if (pressed) {
        // Pressed state (inverted)
        draw_list->AddLine(ImVec2(min.x, max.y), min, dark);
        draw_list->AddLine(min, ImVec2(max.x, min.y), dark);
        draw_list->AddLine(ImVec2(min.x + 1, max.y), ImVec2(min.x + 1, min.y + 1), shadow);
        draw_list->AddLine(ImVec2(min.x + 1, min.y + 1), ImVec2(max.x, min.y + 1), shadow);
    }
    else {
        // Normal raised state
        draw_list->AddLine(ImVec2(min.x, max.y), min, light);
        draw_list->AddLine(min, ImVec2(max.x, min.y), light);
        draw_list->AddLine(ImVec2(max.x, min.y), max, dark);
        draw_list->AddLine(max, ImVec2(min.x, max.y), dark);
        draw_list->AddLine(ImVec2(max.x - 1, min.y + 1), ImVec2(max.x - 1, max.y - 1), shadow);
        draw_list->AddLine(ImVec2(max.x - 1, max.y - 1), ImVec2(min.x + 1, max.y - 1), shadow);
    }
}

// Custom Win98-style button using only public APIs
bool Win98Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0))
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.76f, 0.76f, 0.76f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

    bool result = ImGui::Button(label, size_arg);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    return result;
}

// Helper function to load .ico file as a texture
bool LoadIconTexture(const char* filename, ID3D11Device* device, ID3D11ShaderResourceView** out_srv)
{
    // Load icon file
    HICON hIcon = (HICON)LoadImageA(NULL, filename, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (!hIcon) {
        return false;
    }

    // Get icon info
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) {
        DestroyIcon(hIcon);
        return false;
    }

    // Get bitmap info
    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    // Create a device context
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Create a 32-bit bitmap for the icon
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 32;
    bmi.bmiHeader.biHeight = -32; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        DestroyIcon(hIcon);
        return false;
    }

    // Select bitmap and draw icon
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    DrawIconEx(hdcMem, 0, 0, hIcon, 32, 32, 0, NULL, DI_NORMAL);
    SelectObject(hdcMem, hOldBitmap);

    // Create D3D11 texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 32;
    desc.Height = 32;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pBits;
    initData.SysMemPitch = 32 * 4; // 32 pixels * 4 bytes per pixel

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, &pTexture);

    if (SUCCEEDED(hr)) {
        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
        pTexture->Release();
    }

    // Cleanup
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);

    return SUCCEEDED(hr);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Allocate console for debugging
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();

    std::cout << "=== GobTalk Debug Console ===" << std::endl;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Initialize DECTalk
    if (!dectalk.Initialize("dectalk.dll", WAVE_MAPPER)) {
        MessageBoxA(nullptr, "Failed to initialize DECtalk!\n\nMake sure dectalk.dll is in the same folder as the .exe", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialize GoblinMode (experimental chaos engine)
    std::cout << "[Startup] Initializing Goblin Mode..." << std::endl;
    if (!goblinMode.Initialize("dectalk.dll", WAVE_MAPPER)) {
        std::cout << "[Startup] WARNING: Goblin Mode initialization failed! Goblin mode will be disabled." << std::endl;
    }

    // Initialize message handler
    std::cout << "[Startup] Initializing message handler..." << std::endl;
    messageHandler = std::make_unique<TwitchMessageHandler>(dectalk);
    messageHandler->SetTwitchAuth(&twitchAuth);
    messageHandler->SetGoblinMode(&goblinMode);  // Connect Goblin Mode system
    messageHandler->SetTTSMode(TTSMode::KeepUp); // Default to Pace mode
    messageHandler->bannedUsernames = &bannedUsernames;  // Connect filter lists
    messageHandler->bannedWords = &bannedWords;

    // Set up callbacks from TwitchAuth to message handler
    twitchAuth.SetOnRawMessageCallback([](const std::string& rawMsg) {
        if (messageHandler) {
            messageHandler->OnRawIRCMessage(rawMsg);
        }
        });

    // Set up EventSub follower callback
    eventSub.SetOnFollowCallback([](const std::string& username) {
        if (messageHandler) {
            messageHandler->OnNewFollower(username);
        }
        });

    // Set up EventSub channel point redemption callback
    eventSub.SetOnChannelPointRedemptionCallback([](const std::string& username, const std::string& rewardTitle, const std::string& userInput) {
        if (messageHandler && messageHandler->enableChannelPointRedeems) {
            // Only read if there's a message
            if (!userInput.empty()) {
                std::string message;

                // Respect the "Read Username" toggle
                if (messageHandler->readUsername) {
                    message = username + " says: " + userInput;
                }
                else {
                    message = userInput;
                }

                std::cout << "[Main] Channel point TTS: " << message << std::endl;
                dectalk.Speak(message);
            }
        }
        });

    // Set up status callback to connect EventSub when Twitch connects
    twitchAuth.SetOnStatusCallback([](ConnectionStatus status, const std::string& message) {
        if (status == ConnectionStatus::Connected) {
            std::cout << "[Main] Twitch connected, starting EventSub..." << std::endl;

            // Set EventSub credentials
            eventSub.SetCredentials(
                twitchAuth.GetClientId(),
                twitchAuth.GetUserId(),
                twitchAuth.GetAccessToken()
            );

            // Connect EventSub in a separate thread
            std::thread([]() {
                if (eventSub.Connect()) {
                    std::cout << "[Main] EventSub connected successfully" << std::endl;
                }
                else {
                    std::cout << "[Main] EventSub connection failed" << std::endl;
                }
                }).detach();
        }
        else if (status == ConnectionStatus::Error) {
            std::cout << "[Main] Twitch connection error: " << message << std::endl;
            // Don't try to connect EventSub if IRC failed
        }
        });

    // Start the message handler
    messageHandler->Start();
    std::cout << "[Startup] Message handler started" << std::endl;

    // Load settings
    LoadSettings();

    // Create borderless application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr,
        (HBRUSH)(COLOR_BTNFACE + 1), nullptr, L"Gobtalk", nullptr }; // Set gray background
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Gobtalk - DECtalk Interface",
        WS_POPUP | WS_THICKFRAME, // Borderless but resizable
        100, 100, 600, 500, nullptr, nullptr, wc.hInstance, nullptr);

    // Remove rounded corners (Windows 11)
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Font rendering settings for sharper text
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;  // Horizontal oversampling for sharper rendering
    fontConfig.OversampleV = 2;  // Vertical oversampling
    fontConfig.PixelSnapH = true; // Snap to pixel grid for crisp edges

    // Load MS Sans Serif fonts at 14px for better clarity
    ImFont* fontRegular = nullptr;
    ImFont* fontBold = nullptr;

    // Try to load custom fonts
    fontRegular = io.Fonts->AddFontFromFileTTF("MS Sans Serif.ttf", 12.0f, &fontConfig);
    fontBold = io.Fonts->AddFontFromFileTTF("MS Sans Serif Bold.ttf", 14.0f, &fontConfig);

    // Optional: Load a larger font specifically for tooltips (uncomment to enable)
    // ImFont* tooltipFont = io.Fonts->AddFontFromFileTTF("MS Sans Serif.ttf", 16.0f, &fontConfig);
    // if (tooltipFont) {
    //     TOOLTIP_FONT = tooltipFont;  // Set global tooltip font
    // }

    // If either font failed to load, use default font for both
    if (!fontRegular || !fontBold) {
        io.Fonts->Clear(); // Clear any partial loads
        fontConfig.SizePixels = 14.0f;
        fontRegular = io.Fonts->AddFontDefault(&fontConfig);
        fontBold = fontRegular; // Use same font for both

        // Optional: Show warning message (only once)
        static bool shownWarning = false;
        if (!shownWarning) {
            MessageBoxA(nullptr,
                "MS Sans Serif fonts not found - using default font.\n\n"
                "For authentic Win98 look, place these files next to the .exe:\n"
                "- MS Sans Serif.ttf\n"
                "- MS Sans Serif Bold.ttf",
                "Font Warning", MB_OK | MB_ICONWARNING);
            shownWarning = true;
        }
    }

    // Apply Win98 style
    ApplyWin98Style();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load connection status icons
    LoadIconTexture("conn_pcs_no_network_32x32.ico", g_pd3dDevice, &g_iconNoNetwork);
    LoadIconTexture("conn_pcs_on_on_32x32.ico", g_pd3dDevice, &g_iconConnected);

    // Auto-connect to Twitch on startup if already authenticated
    std::cout << "[Startup] Checking for saved Twitch credentials..." << std::endl;
    if (twitchAuth.IsAuthenticated()) {
        std::cout << "[Startup] Found saved credentials for user: " << twitchAuth.GetUsername() << std::endl;
        std::cout << "[Startup] Auto-connecting to Twitch..." << std::endl;

        // Connect in a separate thread so UI doesn't block
        std::thread([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait 1 second for UI to fully initialize
            twitchAuth.Connect(twitchAuth.GetUsername());
            }).detach();
    }
    else {
        std::cout << "[Startup] No saved credentials found. Click 'Connect To Twitch' to authenticate." << std::endl;
    }

    // Main loop
    bool done = false;
    bool isDragging = false;
    ImVec2 dragOffset;

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Use regular font for all UI (title bar will override with bold)
        ImGui::PushFont(fontRegular);

        // Get window size
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 windowSize = viewport->Size;

        // Full-window ImGui container (no default title bar)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(windowSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("MainWindow", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // ===== WIN98 WINDOW STRUCTURE =====
        const float outerBorderWidth = 4;      // Outer raised border around ENTIRE window
        const float titleBarHeight = 20;       // Title bar height (slightly taller for 14px font)
        const float menuBarHeight = 20;        // Menu bar height (increased for readability)
        const float tabBarHeight = 23;         // Tab bar height (increased for readability)
        // ==================================

        // STEP 1: Draw OUTER RAISED BORDER (all 4 sides including top)
        ImU32 lightOuter = IM_COL32(255, 255, 255, 255);
        ImU32 lightInner = IM_COL32(223, 223, 223, 255);
        ImU32 darkInner = IM_COL32(128, 128, 128, 255);
        ImU32 darkOuter = IM_COL32(0, 0, 0, 255);

        // Top border (4 pixels)
        draw_list->AddLine(ImVec2(0, 0), ImVec2(windowSize.x, 0), lightOuter, 1.0f);
        draw_list->AddLine(ImVec2(0, 1), ImVec2(windowSize.x, 1), lightInner, 1.0f);
        draw_list->AddLine(ImVec2(0, 2), ImVec2(windowSize.x, 2), IM_COL32(240, 240, 240, 255), 1.0f);
        draw_list->AddLine(ImVec2(0, 3), ImVec2(windowSize.x, 3), IM_COL32(192, 192, 192, 255), 1.0f);

        // Left border (4 pixels)
        draw_list->AddLine(ImVec2(0, 0), ImVec2(0, windowSize.y), lightOuter, 1.0f);
        draw_list->AddLine(ImVec2(1, 0), ImVec2(1, windowSize.y), lightInner, 1.0f);
        draw_list->AddLine(ImVec2(2, 0), ImVec2(2, windowSize.y), IM_COL32(240, 240, 240, 255), 1.0f);
        draw_list->AddLine(ImVec2(3, 0), ImVec2(3, windowSize.y), IM_COL32(192, 192, 192, 255), 1.0f);

        // Right border (4 pixels)
        draw_list->AddLine(ImVec2(windowSize.x - 1, 0), ImVec2(windowSize.x - 1, windowSize.y), darkOuter, 1.0f);
        draw_list->AddLine(ImVec2(windowSize.x - 2, 0), ImVec2(windowSize.x - 2, windowSize.y), darkInner, 1.0f);
        draw_list->AddLine(ImVec2(windowSize.x - 3, 0), ImVec2(windowSize.x - 3, windowSize.y), IM_COL32(160, 160, 160, 255), 1.0f);
        draw_list->AddLine(ImVec2(windowSize.x - 4, 0), ImVec2(windowSize.x - 4, windowSize.y), IM_COL32(192, 192, 192, 255), 1.0f);

        // Bottom border (4 pixels)
        draw_list->AddLine(ImVec2(0, windowSize.y - 1), ImVec2(windowSize.x, windowSize.y - 1), darkOuter, 1.0f);
        draw_list->AddLine(ImVec2(0, windowSize.y - 2), ImVec2(windowSize.x, windowSize.y - 2), darkInner, 1.0f);
        draw_list->AddLine(ImVec2(0, windowSize.y - 3), ImVec2(windowSize.x, windowSize.y - 3), IM_COL32(160, 160, 160, 255), 1.0f);
        draw_list->AddLine(ImVec2(0, windowSize.y - 4), ImVec2(windowSize.x, windowSize.y - 4), IM_COL32(192, 192, 192, 255), 1.0f);

        // STEP 2: Title Bar (INSIDE the outer border)
        ImVec2 titleBarMin = ImVec2(outerBorderWidth, outerBorderWidth);
        ImVec2 titleBarMax = ImVec2(windowSize.x - outerBorderWidth, outerBorderWidth + titleBarHeight);

        // Title bar gradient (Win98 blue)
        ImU32 titleColorStart = IM_COL32(0, 0, 128, 255);
        ImU32 titleColorEnd = IM_COL32(16, 132, 208, 255);
        draw_list->AddRectFilledMultiColor(titleBarMin, titleBarMax,
            titleColorStart, titleColorEnd, titleColorEnd, titleColorStart);

        // Title bar text (using BOLD font at 14px)
        const char* titleText = "Gobtalk - DECtalk Interface";
        draw_list->AddText(fontBold, 14.0f, ImVec2(titleBarMin.x + 8, titleBarMin.y + (titleBarHeight - 14.0f) * 0.5f),
            IM_COL32(255, 255, 255, 255), titleText);

        // Title bar buttons (X, Maximize, Minimize)
        const float buttonSize = 16.0f;
        const float buttonY = titleBarMin.y + (titleBarHeight - buttonSize) * 0.5f;
        float buttonX = titleBarMax.x - 22.0f;

        // Close button (X)
        ImVec2 closeBtnMin = ImVec2(buttonX, buttonY);
        ImVec2 closeBtnMax = ImVec2(buttonX + buttonSize, buttonY + buttonSize);
        bool closeHovered = ImGui::IsMouseHoveringRect(closeBtnMin, closeBtnMax);
        bool closeClicked = closeHovered && ImGui::IsMouseClicked(0);

        draw_list->AddRectFilled(closeBtnMin, closeBtnMax,
            closeHovered ? IM_COL32(220, 220, 220, 255) : IM_COL32(192, 192, 192, 255));
        DrawWin98Border(draw_list, closeBtnMin, closeBtnMax, closeClicked);

        // X symbol (thin like Win98)
        float crossPad = 4.0f;
        draw_list->AddLine(ImVec2(closeBtnMin.x + crossPad, closeBtnMin.y + crossPad),
            ImVec2(closeBtnMax.x - crossPad, closeBtnMax.y - crossPad), IM_COL32(0, 0, 0, 255), 1.0f);
        draw_list->AddLine(ImVec2(closeBtnMax.x - crossPad, closeBtnMin.y + crossPad),
            ImVec2(closeBtnMin.x + crossPad, closeBtnMax.y - crossPad), IM_COL32(0, 0, 0, 255), 1.0f);

        if (closeClicked) {
            done = true;
        }

        // Maximize button
        buttonX -= 18.0f;
        ImVec2 maxBtnMin = ImVec2(buttonX, buttonY);
        ImVec2 maxBtnMax = ImVec2(buttonX + buttonSize, buttonY + buttonSize);
        bool maxHovered = ImGui::IsMouseHoveringRect(maxBtnMin, maxBtnMax);
        bool maxClicked = maxHovered && ImGui::IsMouseClicked(0);

        draw_list->AddRectFilled(maxBtnMin, maxBtnMax,
            maxHovered ? IM_COL32(220, 220, 220, 255) : IM_COL32(192, 192, 192, 255));
        DrawWin98Border(draw_list, maxBtnMin, maxBtnMax, maxClicked);

        // Square symbol (thin like Win98)
        float sqPad = 4.0f;
        draw_list->AddRect(ImVec2(maxBtnMin.x + sqPad, maxBtnMin.y + sqPad),
            ImVec2(maxBtnMax.x - sqPad, maxBtnMax.y - sqPad), IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);

        if (maxClicked) {
            if (isMaximized) {
                WINDOWPLACEMENT wp = {};
                wp.length = sizeof(WINDOWPLACEMENT);
                wp.showCmd = SW_RESTORE;
                wp.rcNormalPosition = restoreRect;
                SetWindowPlacement(hwnd, &wp);
                isMaximized = false;
            }
            else {
                GetWindowRect(hwnd, &restoreRect);
                MONITORINFO mi = { sizeof(mi) };
                GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
                SetWindowPos(hwnd, NULL, mi.rcWork.left, mi.rcWork.top,
                    mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top, 0);
                isMaximized = true;
            }
        }

        // Minimize button
        buttonX -= 18.0f;
        ImVec2 minBtnMin = ImVec2(buttonX, buttonY);
        ImVec2 minBtnMax = ImVec2(buttonX + buttonSize, buttonY + buttonSize);
        bool minHovered = ImGui::IsMouseHoveringRect(minBtnMin, minBtnMax);
        bool minClicked = minHovered && ImGui::IsMouseClicked(0);

        draw_list->AddRectFilled(minBtnMin, minBtnMax,
            minHovered ? IM_COL32(220, 220, 220, 255) : IM_COL32(192, 192, 192, 255));
        DrawWin98Border(draw_list, minBtnMin, minBtnMax, minClicked);

        // Line symbol (thin like Win98)
        draw_list->AddLine(ImVec2(minBtnMin.x + 4, minBtnMax.y - 6),
            ImVec2(minBtnMax.x - 4, minBtnMax.y - 6), IM_COL32(0, 0, 0, 255), 1.0f);

        if (minClicked) {
            ShowWindow(hwnd, SW_MINIMIZE);
        }

        // Handle title bar dragging
        ImVec2 mousePos = ImGui::GetMousePos();
        bool inTitleBar = mousePos.y >= titleBarMin.y &&
            mousePos.y < titleBarMax.y &&
            mousePos.x >= titleBarMin.x &&
            mousePos.x < buttonX - 10;

        if (inTitleBar && ImGui::IsMouseClicked(0)) {
            isDragging = true;
            POINT pt;
            GetCursorPos(&pt);
            RECT rect;
            GetWindowRect(hwnd, &rect);
            dragOffset = ImVec2((float)(pt.x - rect.left), (float)(pt.y - rect.top));
        }

        if (isDragging) {
            if (ImGui::IsMouseDown(0)) {
                POINT pt;
                GetCursorPos(&pt);
                SetWindowPos(hwnd, NULL, pt.x - (int)dragOffset.x, pt.y - (int)dragOffset.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            else {
                isDragging = false;
            }
        }

        // STEP 3: Menu Bar (File, Edit, About)
        float menuBarY = outerBorderWidth + titleBarHeight;
        ImVec2 menuBarMin = ImVec2(outerBorderWidth, menuBarY);
        ImVec2 menuBarSize = ImVec2(windowSize.x - (outerBorderWidth * 2), menuBarHeight);

        // Gray background for menu bar
        draw_list->AddRectFilled(menuBarMin, ImVec2(menuBarMin.x + menuBarSize.x, menuBarMin.y + menuBarSize.y),
            IM_COL32(192, 192, 192, 255));

        // Menu items - Win98 style (flat by default, raised on hover)
        const char* menuItems[] = { "File", "Edit", "Help" };
        float menuX = menuBarMin.x + 4;

        // ===== MENU ITEM PADDING (adjust border spacing here) =====
        const float menuPaddingX = 8;      // Horizontal padding (left/right of text) - BIGGER = MORE SPACE
        const float menuPaddingY = 3;      // Vertical padding (top/bottom of text) - BIGGER = MORE SPACE
        // ==========================================================

        for (int i = 0; i < IM_ARRAYSIZE(menuItems); i++) {
            ImVec2 textSize = ImGui::CalcTextSize(menuItems[i]);

            // Calculate text position first (centered in menu bar)
            ImVec2 textPos = ImVec2(menuX + menuPaddingX, menuBarMin.y + (menuBarSize.y - textSize.y) * 0.5f);

            // Calculate border box AROUND the text (with padding)
            ImVec2 itemMin = ImVec2(textPos.x - menuPaddingX, textPos.y - menuPaddingY);
            ImVec2 itemMax = ImVec2(textPos.x + textSize.x + menuPaddingX, textPos.y + textSize.y + menuPaddingY);

            float menuItemWidth = itemMax.x - itemMin.x;

            // Check hover
            bool isHovered = ImGui::IsMouseHoveringRect(itemMin, itemMax);
            bool isClicked = isHovered && ImGui::IsMouseClicked(0);

            if (isHovered) {
                // Draw raised 3D border on hover
                DrawWin98Border(draw_list, itemMin, itemMax, false);
            }

            // Draw text (with underlined first letter for hotkey)
            draw_list->AddText(textPos, IM_COL32(0, 0, 0, 255), menuItems[i]);

            // Draw underline under first letter (hotkey indicator)
            float underlineY = textPos.y + textSize.y;
            ImVec2 firstCharSize = ImGui::CalcTextSize(menuItems[i], menuItems[i] + 1);
            draw_list->AddLine(ImVec2(textPos.x, underlineY),
                ImVec2(textPos.x + firstCharSize.x, underlineY),
                IM_COL32(0, 0, 0, 255), 1.0f);

            // Click handling
            if (isClicked) {
                if (openMenuIndex == i) {
                    openMenuIndex = -1;  // Close if clicking same menu
                    justOpenedMenu = false;
                }
                else {
                    openMenuIndex = i;  // Open menu
                    justOpenedMenu = true;  // Flag to prevent immediate close
                }
            }

            // Draw dropdown if this menu is open
            if (openMenuIndex == i) {
                ImVec2 dropdownMin = ImVec2(itemMin.x, itemMax.y);
                float dropdownWidth = 180;

                // Menu items for each dropdown
                const char* fileMenuItems[] = { "Save Settings", "Load Settings", "Backup Settings", nullptr, "Exit" };
                const char* editMenuItems[] = { "Clear Filters", "Reset to Defaults" };
                const char* helpMenuItems[] = { "About Gobtalk", nullptr, "GitHub Project", "My Website" };

                const char** menuItemsList = nullptr;
                int menuItemsCount = 0;

                if (i == 0) { // File
                    menuItemsList = fileMenuItems;
                    menuItemsCount = 5;
                }
                else if (i == 1) { // Edit
                    menuItemsList = editMenuItems;
                    menuItemsCount = 2;
                }
                else if (i == 2) { // Help
                    menuItemsList = helpMenuItems;
                    menuItemsCount = 4;
                }

                if (menuItemsList) {
                    float itemHeight = 20;
                    float dropdownHeight = itemHeight * menuItemsCount;
                    ImVec2 dropdownMax = ImVec2(dropdownMin.x + dropdownWidth, dropdownMin.y + dropdownHeight);

                    // Dropdown background
                    draw_list->AddRectFilled(dropdownMin, dropdownMax, IM_COL32(192, 192, 192, 255));
                    DrawWin98Border(draw_list, dropdownMin, dropdownMax, false);

                    // Menu items
                    for (int j = 0; j < menuItemsCount; j++) {
                        if (menuItemsList[j] == nullptr) {
                            // Separator
                            float sepY = dropdownMin.y + (j * itemHeight) + itemHeight / 2;
                            draw_list->AddLine(ImVec2(dropdownMin.x + 4, sepY),
                                ImVec2(dropdownMax.x - 4, sepY),
                                IM_COL32(128, 128, 128, 255), 1.0f);
                            continue;
                        }

                        ImVec2 menuItemMin = ImVec2(dropdownMin.x, dropdownMin.y + (j * itemHeight));
                        ImVec2 menuItemMax = ImVec2(dropdownMax.x, menuItemMin.y + itemHeight);

                        bool menuItemHovered = ImGui::IsMouseHoveringRect(menuItemMin, menuItemMax);
                        bool menuItemClicked = menuItemHovered && ImGui::IsMouseClicked(0);

                        // Hover highlight
                        if (menuItemHovered) {
                            draw_list->AddRectFilled(menuItemMin, menuItemMax, IM_COL32(0, 0, 128, 255));
                        }

                        // Text
                        ImU32 menuTextColor = menuItemHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(0, 0, 0, 255);
                        draw_list->AddText(ImVec2(menuItemMin.x + 8, menuItemMin.y + 3),
                            menuTextColor, menuItemsList[j]);

                        // Handle menu item clicks
                        if (menuItemClicked) {
                            openMenuIndex = -1;  // Close menu
                            justOpenedMenu = false;  // Reset flag

                            // Execute menu action
                            if (i == 0) { // File menu
                                if (j == 0) { SaveSettings(); }  // Save Settings
                                else if (j == 1) { LoadSettings(); }  // Load Settings
                                else if (j == 2) { // Backup Settings
                                    char timestamp[64];
                                    time_t now = time(nullptr);
                                    struct tm timeinfo;
                                    localtime_s(&timeinfo, &now);
                                    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeinfo);
                                    std::string backupFile = "gobtalk_backup_" + std::string(timestamp) + ".ini";
                                    SaveSettings(backupFile.c_str());
                                }
                                else if (j == 4) { done = true; } // Exit
                            }
                            else if (i == 1) { // Edit menu
                                if (j == 0) { // Clear Filters
                                    bannedUsernames.clear();
                                    bannedWords.clear();
                                    bannedUsernamesText[0] = '\0';
                                    bannedWordsText[0] = '\0';
                                }
                                else if (j == 1) { // Reset to Defaults
                                    selectedVoice = 0;
                                    dectalk.SetVoice(0);
                                    goblinMode.SetVoice(0);
                                    volume = 100;
                                    dectalk.SetVolume(100);
                                    goblinMode.SetVolume(100);
                                    selectedQueueMode = 1; // Pace mode
                                    if (messageHandler) {
                                        messageHandler->SetTTSMode(TTSMode::KeepUp);
                                        messageHandler->enableChatMessages = true;
                                        messageHandler->enableSayCommand = false;
                                        messageHandler->readUsername = true;
                                        messageHandler->roleFilterMode = TwitchMessageHandler::RoleFilterMode::Everyone;
                                        messageHandler->enableSubscriberAnnouncements = true;
                                        messageHandler->enableFollowerAnnouncements = true;
                                        messageHandler->enableBitsAnnouncements = true;
                                        messageHandler->enableChannelPointRedeems = true;
                                    }
                                    bannedUsernames.clear();
                                    bannedWords.clear();
                                    bannedUsernamesText[0] = '\0';
                                    bannedWordsText[0] = '\0';
                                    std::cout << "[Settings] Reset to defaults" << std::endl;
                                }
                            }
                            else if (i == 2) { // Help menu
                                if (j == 0) { // About Gobtalk
                                    showAboutDialog = true;
                                }
                                else if (j == 2) { // GitHub Project
                                    ShellExecuteA(NULL, "open", "https://github.com/yourusername/gobtalk", NULL, NULL, SW_SHOWNORMAL);
                                }
                                else if (j == 3) { // My Website
                                    ShellExecuteA(NULL, "open", "https://yourwebsite.com", NULL, NULL, SW_SHOWNORMAL);
                                }
                            }
                        }
                    }
                }
            }

            menuX += menuItemWidth + 1;
        }

        // Close menu if clicked outside menu area (but not on the same frame we opened it)
        if (openMenuIndex != -1 && ImGui::IsMouseClicked(0) && !justOpenedMenu) {
            // Get mouse position
            ImVec2 mousePos = ImGui::GetMousePos();

            // Check if click was in menu bar area
            bool clickedInMenuBar = (mousePos.y >= menuBarMin.y &&
                mousePos.y <= menuBarMin.y + menuBarSize.y);

            // Check if click was in dropdown area (approximately)
            bool clickedInDropdown = (mousePos.y > menuBarMin.y + menuBarSize.y &&
                mousePos.y < menuBarMin.y + menuBarSize.y + 200);

            // Close menu only if clicked outside both menu bar and dropdown
            if (!clickedInMenuBar && !clickedInDropdown) {
                openMenuIndex = -1;
            }
        }

        // Reset the justOpenedMenu flag after checking
        if (justOpenedMenu && !ImGui::IsMouseClicked(0)) {
            justOpenedMenu = false;
        }

        // STEP 4: Tab Bar (Win98 paper-tab style)
        static int selectedTab = 0;
        const char* tabNames[] = { "Voice Settings", "Options", "Connections", "Filters" };
        float tabBarY = menuBarY + menuBarHeight;
        ImVec2 tabBarMin = ImVec2(outerBorderWidth, tabBarY);

        // Draw tab background area
        draw_list->AddRectFilled(ImVec2(tabBarMin.x, tabBarMin.y),
            ImVec2(windowSize.x - outerBorderWidth, tabBarMin.y + tabBarHeight),
            IM_COL32(192, 192, 192, 255));

        // Draw tabs
        float tabX = tabBarMin.x + 4;
        for (int i = 0; i < IM_ARRAYSIZE(tabNames); i++) {
            ImVec2 tabTextSize = ImGui::CalcTextSize(tabNames[i]);
            float tabWidth = tabTextSize.x + 24;
            bool isSelected = (selectedTab == i);

            ImVec2 tabMin = ImVec2(tabX, tabBarMin.y + (isSelected ? 2 : 4));
            ImVec2 tabMax = ImVec2(tabX + tabWidth, tabBarMin.y + tabBarHeight);

            // Tab background - gray for selected, slightly darker for unselected
            ImU32 tabColor = isSelected ? IM_COL32(192, 192, 192, 255) : IM_COL32(192, 192, 192, 255);
            draw_list->AddRectFilled(tabMin, tabMax, tabColor);

            // Tab border (Win98 paper-tab effect)
            if (isSelected) {
                // Selected tab: raised border on top and sides, NO bottom (flows into content)
                // Left edge
                draw_list->AddLine(ImVec2(tabMin.x, tabMax.y), ImVec2(tabMin.x, tabMin.y + 2), IM_COL32(255, 255, 255, 255), 1.0f);
                // Top-left corner
                draw_list->AddLine(ImVec2(tabMin.x, tabMin.y + 2), ImVec2(tabMin.x + 2, tabMin.y), IM_COL32(255, 255, 255, 255), 1.0f);
                // Top edge
                draw_list->AddLine(ImVec2(tabMin.x + 2, tabMin.y), ImVec2(tabMax.x - 2, tabMin.y), IM_COL32(255, 255, 255, 255), 1.0f);
                // Top-right corner
                draw_list->AddLine(ImVec2(tabMax.x - 2, tabMin.y), ImVec2(tabMax.x, tabMin.y + 2), IM_COL32(128, 128, 128, 255), 1.0f);
                // Right edge (outer)
                draw_list->AddLine(ImVec2(tabMax.x, tabMin.y + 2), ImVec2(tabMax.x, tabMax.y), IM_COL32(0, 0, 0, 255), 1.0f);
                // Right edge (inner shadow)
                draw_list->AddLine(ImVec2(tabMax.x - 1, tabMin.y + 2), ImVec2(tabMax.x - 1, tabMax.y), IM_COL32(128, 128, 128, 255), 1.0f);
                // NO BOTTOM BORDER - tab flows seamlessly into content area below
            }
            else {
                // Unselected tab: slightly recessed with all borders
                draw_list->AddLine(ImVec2(tabMin.x, tabMax.y - 1), ImVec2(tabMin.x, tabMin.y + 2), IM_COL32(223, 223, 223, 255), 1.0f); // Left
                draw_list->AddLine(ImVec2(tabMin.x, tabMin.y + 2), ImVec2(tabMin.x + 2, tabMin.y), IM_COL32(223, 223, 223, 255), 1.0f); // Corner
                draw_list->AddLine(ImVec2(tabMin.x + 2, tabMin.y), ImVec2(tabMax.x - 2, tabMin.y), IM_COL32(223, 223, 223, 255), 1.0f); // Top
                draw_list->AddLine(ImVec2(tabMax.x - 2, tabMin.y), ImVec2(tabMax.x, tabMin.y + 2), IM_COL32(128, 128, 128, 255), 1.0f); // Corner
                draw_list->AddLine(ImVec2(tabMax.x, tabMin.y + 2), ImVec2(tabMax.x, tabMax.y - 1), IM_COL32(128, 128, 128, 255), 1.0f); // Right
                draw_list->AddLine(ImVec2(tabMin.x, tabMax.y - 1), ImVec2(tabMax.x, tabMax.y - 1), IM_COL32(128, 128, 128, 255), 1.0f); // Bottom
            }

            // Tab text - black for selected, slightly gray for unselected
            ImU32 textColor = isSelected ? IM_COL32(0, 0, 0, 255) : IM_COL32(64, 64, 64, 255);
            draw_list->AddText(ImVec2(tabX + 12, tabMin.y + (tabMax.y - tabMin.y - tabTextSize.y) * 0.5f),
                textColor, tabNames[i]);

            // Click detection
            bool tabHovered = ImGui::IsMouseHoveringRect(tabMin, tabMax);
            if (tabHovered && ImGui::IsMouseClicked(0)) {
                selectedTab = i;
            }

            tabX += tabWidth + 2;
        }

        // STEP 5: Content area (RAISED gray panel - blends with selected tab)
        float contentY = tabBarY + tabBarHeight;
        ImVec2 contentAreaMin = ImVec2(outerBorderWidth + 4, contentY);
        ImVec2 contentAreaMax = ImVec2(windowSize.x - outerBorderWidth - 4, windowSize.y - outerBorderWidth - 4);

        // Gray background for content (RGB 192,192,192 - button face color)
        draw_list->AddRectFilled(contentAreaMin, contentAreaMax, IM_COL32(192, 192, 192, 255));

        // Calculate where selected tab is
        float selectedTabStartX = tabBarMin.x + 4;
        for (int i = 0; i < selectedTab; i++) {
            ImVec2 tabTextSize = ImGui::CalcTextSize(tabNames[i]);
            selectedTabStartX += tabTextSize.x + 24 + 2;
        }
        ImVec2 selectedTabTextSize = ImGui::CalcTextSize(tabNames[selectedTab]);
        float selectedTabEndX = selectedTabStartX + selectedTabTextSize.x + 24;

        // Draw RAISED bevel around content - but skip top border where tab connects
        // (Reusing lightOuter, lightInner, darkInner, darkOuter from window border above)

        // Left border (raised)
        draw_list->AddLine(ImVec2(contentAreaMin.x, contentAreaMax.y - 1), contentAreaMin, lightOuter, 1.0f);
        draw_list->AddLine(ImVec2(contentAreaMin.x + 1, contentAreaMax.y - 2), ImVec2(contentAreaMin.x + 1, contentAreaMin.y + 1), lightInner, 1.0f);

        // Top border (raised) - SKIP the selected tab area
        // Draw from left edge to start of selected tab
        draw_list->AddLine(contentAreaMin, ImVec2(selectedTabStartX + 1, contentAreaMin.y), lightOuter, 1.0f);
        draw_list->AddLine(ImVec2(contentAreaMin.x + 1, contentAreaMin.y + 1), ImVec2(selectedTabStartX + 1, contentAreaMin.y + 1), lightInner, 1.0f);
        // Draw from end of selected tab to right edge
        draw_list->AddLine(ImVec2(selectedTabEndX - 1, contentAreaMin.y), ImVec2(contentAreaMax.x, contentAreaMin.y), lightOuter, 1.0f);
        draw_list->AddLine(ImVec2(selectedTabEndX - 1, contentAreaMin.y + 1), ImVec2(contentAreaMax.x - 1, contentAreaMin.y + 1), lightInner, 1.0f);

        // Right border (raised)
        draw_list->AddLine(ImVec2(contentAreaMax.x - 1, contentAreaMin.y), ImVec2(contentAreaMax.x - 1, contentAreaMax.y), darkOuter, 1.0f);
        draw_list->AddLine(ImVec2(contentAreaMax.x - 2, contentAreaMin.y + 1), ImVec2(contentAreaMax.x - 2, contentAreaMax.y - 1), darkInner, 1.0f);

        // Bottom border (raised)
        draw_list->AddLine(ImVec2(contentAreaMax.x - 1, contentAreaMax.y - 1), ImVec2(contentAreaMin.x, contentAreaMax.y - 1), darkOuter, 1.0f);
        draw_list->AddLine(ImVec2(contentAreaMax.x - 2, contentAreaMax.y - 2), ImVec2(contentAreaMin.x + 1, contentAreaMax.y - 2), darkInner, 1.0f);

        ImVec2 contentPos = ImVec2(contentAreaMin.x + 8, contentAreaMin.y + 8);
        ImVec2 contentSize = ImVec2(contentAreaMax.x - contentAreaMin.x - 16,
            contentAreaMax.y - contentAreaMin.y - 16);

        ImGui::SetCursorScreenPos(contentPos);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)); // Gray background
        ImGui::BeginChild("TabContent", contentSize, false);

        // Tab content based on selection
        if (selectedTab == 0) {
            // Voice Settings tab content
            ImGui::Text("Voice Selection");
            ImGui::Separator();

            for (int i = 0; i < IM_ARRAYSIZE(voiceNames); i++)
            {
                if (ImGui::RadioButton(voiceNames[i], &selectedVoice, i))
                {
                    dectalk.SetVoice(i);
                    goblinMode.SetVoice(i);  // Sync to Goblin Mode
                }
                if ((i + 1) % 3 != 0) ImGui::SameLine();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Voice Parameters");
            ImGui::Text("Volume:");
            ImGui::SameLine(80);
            if (ImGui::SliderInt("##volume", &volume, 0, 100))
            {
                dectalk.SetVolume(volume);
                goblinMode.SetVolume(volume);  // Sync to Goblin Mode
            }

            ImGui::Spacing();

            ImGui::Text("Queue Mode:");
            ImGui::SameLine(80);
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("##queuemode", &selectedQueueMode, queueModeNames, IM_ARRAYSIZE(queueModeNames)))
            {
                // Map UI selection to TTSMode enum
                TTSMode mode;
                switch (selectedQueueMode) {
                case 0: mode = TTSMode::Queue; break;
                case 1: mode = TTSMode::KeepUp; break;  // Pace
                case 2: mode = TTSMode::Goblin; break;
                default: mode = TTSMode::KeepUp; break;
                }
                messageHandler->SetTTSMode(mode);
            }
            ShowTooltip("Queue: Read all messages in order\nPace: Skip old messages, read latest\nGoblin: Read all messages simultaneously (chaos!)");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Text to Speak");
            ImGui::InputTextMultiline("##text", textBuffer, IM_ARRAYSIZE(textBuffer), ImVec2(-1, 100));

            ImGui::Spacing();

            // Speak button on the left
            if (Win98Button("Speak", ImVec2(200, 30)))
            {
                dectalk.Speak(textBuffer);
            }

            // Stop button on the same line, to the right
            ImGui::SameLine();
            if (Win98Button("Stop", ImVec2(200, 30)))
            {
                dectalk.Stop();              // Stop current audio
                messageHandler->ClearQueue(); // Clear pending messages
            }

            // Skip button underneath Speak
            if (Win98Button("Skip", ImVec2(200, 30)))
            {
                dectalk.Skip();
            }
        }
        else if (selectedTab == 2) {
            // Connections tab content
            ImGui::Text("Twitch Connection");
            ImGui::Separator();
            ImGui::Spacing();

            // Get current status
            ConnectionStatus currentStatus = twitchAuth.GetStatus();
            bool isConnected = (currentStatus == ConnectionStatus::Connected);
            bool isConnecting = (currentStatus == ConnectionStatus::Connecting);
            bool disconnecting = twitchAuth.IsDisconnecting() || eventSub.IsDisconnecting();

            // ========== BUTTONS SIDE BY SIDE ==========
            const char* connectButtonText = isConnected ? "Connected" :
                isConnecting ? "Connecting..." :
                "Connect To Twitch";

            // Connect button (left)
            bool connectEnabled = !isConnecting && !disconnecting;
            if (!connectEnabled) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            }

            if (Win98Button(connectButtonText, ImVec2(150, 30)) && connectEnabled)
            {
                if (!twitchAuth.IsAuthenticated()) {
                    std::cout << "[UI] Starting authentication flow..." << std::endl;
                    twitchAuth.StartAuthentication();
                }
                else if (!isConnected && !isConnecting) {
                    std::cout << "[UI] Connecting to channel: " << twitchAuth.GetUsername() << std::endl;
                    twitchAuth.Connect(twitchAuth.GetUsername());
                }
            }

            if (!connectEnabled) {
                ImGui::PopStyleVar();
            }

            // Disconnect button (right, same line)
            if (twitchAuth.IsAuthenticated()) {
                ImGui::SameLine();

                bool disconnectEnabled = !disconnecting && !isConnecting;
                if (!disconnectEnabled) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
                }

                if (Win98Button("Disconnect", ImVec2(150, 30)) && disconnectEnabled) {
                    std::cout << "[UI] Starting disconnect..." << std::endl;

                    std::thread([]() {
                        twitchAuth.Disconnect();
                        eventSub.Disconnect();
                        twitchAuth.RevokeAuthentication();
                        std::cout << "[UI] Disconnect complete!" << std::endl;
                        }).detach();
                }

                if (!disconnectEnabled) {
                    ImGui::PopStyleVar();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ========== STATUS WITH ICON ==========
            ImGui::Text("Status:");
            ImGui::SameLine();

            // Display icon
            ID3D11ShaderResourceView* statusIcon = isConnected ? g_iconConnected : g_iconNoNetwork;
            if (statusIcon) {
                ImGui::Image((void*)statusIcon, ImVec2(16, 16));
                ImGui::SameLine();
            }

            // Status text
            if (isConnected) {
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Connected");
            }
            else if (isConnecting) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Connecting...");
            }
            else if (disconnecting) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Disconnecting...");
            }
            else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Disconnected");
            }

            ImGui::Spacing();

            // EventSub status
            bool eventSubConnected = eventSub.IsConnected();
            ImGui::Text("EventSub:");
            ImGui::SameLine();
            if (isConnected && eventSubConnected) {
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Active");
            }
            else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Inactive");
            }
        }
        else if (selectedTab == 1) {
            // Options tab content
            ImGui::Text("TTS Feature Toggles");
            ImGui::Separator();
            ImGui::Spacing();

            // Chat Messages and !say Command on same line
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Read Chat Messages", &messageHandler->enableChatMessages);
            END_CHECKBOX_STYLE();
            ShowTooltip("Must be ON for the bot to process chat messages and !say commands");

            ImGui::SameLine(250); // Position to the right

            // Grey out everything below if Read Chat Messages is OFF
            if (!messageHandler->enableChatMessages) {
                ImGui::BeginDisabled();
            }

            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Require !say Command", &messageHandler->enableSayCommand);
            END_CHECKBOX_STYLE();
            ShowTooltip("When ON: Users MUST use !say to have messages read\nWhen OFF: All messages are read normally");

            ImGui::Spacing();

            // Read Username toggle
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Read Username", &messageHandler->readUsername);
            END_CHECKBOX_STYLE();
            ShowTooltip("When ON: Says \"Username says: message\" (for all messages including !say)\nWhen OFF: Just says the message");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Role Filtering
            ImGui::Text("Role Filtering");
            ImGui::Spacing();

            // Dropdown for role filter mode
            static int roleFilterSelection = 0; // 0 = Everyone, 1 = Custom
            const char* roleFilterOptions[] = { "Everyone", "Custom" };

            ImGui::Text("Read messages from:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("##rolefilter", &roleFilterSelection, roleFilterOptions, IM_ARRAYSIZE(roleFilterOptions))) {
                messageHandler->roleFilterMode = (roleFilterSelection == 0) ?
                    TwitchMessageHandler::RoleFilterMode::Everyone :
                    TwitchMessageHandler::RoleFilterMode::Custom;
            }

            ShowTooltip("Everyone: Read all messages\nCustom: Only read from selected roles");

            // Always show role checkboxes (grey them out instead of hiding)
            ImGui::Spacing();
            ImGui::Indent(20);

            ImGui::Text("Allow messages from:");
            ImGui::Spacing();

            // Disable role checkboxes when "Everyone" is selected
            if (roleFilterSelection == 0) {
                ImGui::BeginDisabled();
            }

            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Followers", &messageHandler->allowFollowers);
            END_CHECKBOX_STYLE();
            ShowTooltip("Allow messages from followers (currently allows everyone - EventSub needed)");

            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Subscribers##rolefilter", &messageHandler->allowSubscribers);
            END_CHECKBOX_STYLE();
            ShowTooltip("Allow messages from channel subscribers");

            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Turbo Users", &messageHandler->allowTurbo);
            END_CHECKBOX_STYLE();
            ShowTooltip("Allow messages from Twitch Turbo users");

            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Moderators", &messageHandler->allowMods);
            END_CHECKBOX_STYLE();
            ShowTooltip("Allow messages from channel moderators");

            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("VIPs", &messageHandler->allowVIPs);
            END_CHECKBOX_STYLE();
            ShowTooltip("Allow messages from channel VIPs");

            // End role checkbox disabled state
            if (roleFilterSelection == 0) {
                ImGui::EndDisabled();
            }

            ImGui::Unindent(20);

            // End "Read Chat Messages" disabled state
            if (!messageHandler->enableChatMessages) {
                ImGui::EndDisabled();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Announcements
            ImGui::Text("Announcements");
            ImGui::Spacing();

            // Subscriber Announcements
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Subscribers##announcements", &messageHandler->enableSubscriberAnnouncements);
            END_CHECKBOX_STYLE();
            ShowTooltip("Announce new subscriptions and resubscriptions");

            // Always show options, grey them out when disabled
            ImGui::Indent(20);
            if (!messageHandler->enableSubscriberAnnouncements) {
                ImGui::BeginDisabled();
            }

            ImGui::Text("Subscription Message:");
            ImGui::SetNextItemWidth(-1); // Full width
            char subBuffer[256];
            strncpy_s(subBuffer, messageHandler->subscriberMessage.c_str(), sizeof(subBuffer) - 1);
            if (ImGui::InputText("##submsg", subBuffer, sizeof(subBuffer))) {
                messageHandler->subscriberMessage = subBuffer;
            }
            ShowTooltip("Use {username} as placeholder. Example: {username} just subscribed!");

            ImGui::Text("Resubscription Message:");
            ImGui::SetNextItemWidth(-1); // Full width
            char resubBuffer[256];
            strncpy_s(resubBuffer, messageHandler->resubscriberMessage.c_str(), sizeof(resubBuffer) - 1);
            if (ImGui::InputText("##resubmsg", resubBuffer, sizeof(resubBuffer))) {
                messageHandler->resubscriberMessage = resubBuffer;
            }
            ShowTooltip("Use {username} and {months} as placeholders. Example: {username} resubscribed for {months} months!");

            if (!messageHandler->enableSubscriberAnnouncements) {
                ImGui::EndDisabled();
            }
            ImGui::Unindent(20);

            ImGui::Spacing();

            // Follower Announcements
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Follower Announcements", &messageHandler->enableFollowerAnnouncements);
            END_CHECKBOX_STYLE();
            ShowTooltip("Announce new followers via Twitch EventSub WebSocket");

            // Always show options, grey them out when disabled
            ImGui::Indent(20);
            if (!messageHandler->enableFollowerAnnouncements) {
                ImGui::BeginDisabled();
            }

            ImGui::Text("Follower Message:");
            ImGui::SetNextItemWidth(-1); // Full width
            char followerBuffer[256];
            strncpy_s(followerBuffer, messageHandler->followerMessage.c_str(), sizeof(followerBuffer) - 1);
            if (ImGui::InputText("##followmsg", followerBuffer, sizeof(followerBuffer))) {
                messageHandler->followerMessage = followerBuffer;
            }
            ShowTooltip("Use {username} as placeholder. Example: {username} just followed!");

            if (!messageHandler->enableFollowerAnnouncements) {
                ImGui::EndDisabled();
            }
            ImGui::Unindent(20);

            ImGui::Spacing();

            // Bits Announcements
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Bits/Cheers", &messageHandler->enableBitsAnnouncements);
            END_CHECKBOX_STYLE();
            ShowTooltip("Announce bit cheers");

            // Always show options, grey them out when disabled
            ImGui::Indent(20);
            if (!messageHandler->enableBitsAnnouncements) {
                ImGui::BeginDisabled();
            }

            ImGui::Text("Minimum Bits:");
            ImGui::SameLine(150);
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("##minbits", &messageHandler->minBitsToAnnounce, 1, 10);
            if (messageHandler->minBitsToAnnounce < 1) messageHandler->minBitsToAnnounce = 1;

            if (!messageHandler->enableBitsAnnouncements) {
                ImGui::EndDisabled();
            }
            ImGui::Unindent(20);

            ImGui::Spacing();

            // Channel Point Redeems
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Channel Point Redeems", &messageHandler->enableChannelPointRedeems);
            END_CHECKBOX_STYLE();
            ShowTooltip("Read messages from specific channel point reward redemptions");

            // Always show options, grey them out when disabled
            ImGui::Indent(20);
            if (!messageHandler->enableChannelPointRedeems) {
                ImGui::BeginDisabled();
            }

            ImGui::Text("Reward Name:");
            ImGui::SetNextItemWidth(-1); // Full width
            if (ImGui::InputText("##rewardname", channelPointRewardName, sizeof(channelPointRewardName))) {
                // Update EventSub filter when text changes
                eventSub.SetRewardNameFilter(channelPointRewardName);
            }
            if (ImGui::IsItemHovered()) {
                BEGIN_TOOLTIP()
                    ImGui::Text("Enter the exact name of the reward to listen for (case-sensitive).");
                ImGui::Text("When this reward is redeemed, the TTS will read the user's message.");
                END_TOOLTIP()
            }

            if (!messageHandler->enableChannelPointRedeems) {
                ImGui::EndDisabled();
            }
            ImGui::Unindent(20);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Future features
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Future Features:");
            ImGui::Spacing();

            bool donationsEnabled = false; // Placeholder
            ImGui::BeginDisabled();
            BEGIN_CHECKBOX_STYLE();
            ImGui::Checkbox("Donations", &donationsEnabled);
            END_CHECKBOX_STYLE();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Coming Soon)");
            ShowTooltip("Announce donations (requires external service integration)");
        }
        else if (selectedTab == 3) {
            // Filters tab content
            ImGui::Text("Username Blacklist");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Block messages from specific usernames (one per line):");
            ImGui::Spacing();

            // Multiline input for usernames
            ImGui::Text("Banned Usernames:");
            if (ImGui::InputTextMultiline("##usernamelist", bannedUsernamesText, sizeof(bannedUsernamesText),
                ImVec2(400, 150), ImGuiInputTextFlags_AllowTabInput)) {
                UpdateBannedUsernamesFromText();
            }
            ShowTooltip("Enter one username per line.\nChanges are applied automatically.");

            ImGui::Spacing();
            ImGui::Text("Current Count: %zu", bannedUsernames.size());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Banned Words section
            ImGui::Text("Banned Words");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Filter messages containing these words (one per line):");
            ImGui::Spacing();

            // Multiline input for words
            ImGui::Text("Banned Words:");
            if (ImGui::InputTextMultiline("##wordlist", bannedWordsText, sizeof(bannedWordsText),
                ImVec2(400, 150), ImGuiInputTextFlags_AllowTabInput)) {
                UpdateBannedWordsFromText();
            }
            ShowTooltip("Enter one word per line.\nChanges are applied automatically.");

            ImGui::Spacing();
            ImGui::Text("Current Count: %zu", bannedWords.size());
        }

        ImGui::EndChild(); // TabContent
        ImGui::PopStyleColor(); // Pop white background

        ImGui::End();
        ImGui::PopStyleVar(3);

        ImGui::PopFont(); // Pop regular font

        // About Dialog (Win98 style modal)
        if (showAboutDialog) {
            ImGui::OpenPopup("About Gobtalk");
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            if (ImGui::BeginPopupModal("About Gobtalk", &showAboutDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PushFont(fontBold);
                ImGui::Text("Gobtalk - DECtalk Interface");
                ImGui::PopFont();

                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Version: 1.0.0");
                ImGui::Text("A Windows 98-styled TTS interface for Twitch");
                ImGui::Spacing();
                ImGui::Text("Features:");
                ImGui::BulletText("DECtalk voice synthesis");
                ImGui::BulletText("Twitch chat integration");
                ImGui::BulletText("Multiple TTS modes (Queue, Pace, Goblin)");
                ImGui::BulletText("Username & word filtering");
                ImGui::BulletText("Customizable announcements");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (Win98Button("OK", ImVec2(120, 25))) {
                    showAboutDialog = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.76f, 0.76f, 0.76f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Auto-save settings before exit
    std::cout << "[Shutdown] Auto-saving settings..." << std::endl;
    SaveSettings();

    // Cleanup
    if (g_iconNoNetwork) { g_iconNoNetwork->Release(); g_iconNoNetwork = nullptr; }
    if (g_iconConnected) { g_iconConnected->Release(); g_iconConnected = nullptr; }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions for D3D11
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_NCCALCSIZE:
        // Tell Windows the entire window is client area (no title bar, no frame)
        if (wParam == TRUE) {
            // Return 0 to indicate the entire window is client area
            return 0;
        }
        break;
    case WM_ERASEBKGND:
    {
        // Paint the window background gray to match ImGui
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        HBRUSH brush = CreateSolidBrush(RGB(194, 194, 194)); // Win98 gray
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}