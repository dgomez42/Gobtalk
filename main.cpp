#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "DECtalkWrapper.h"
#include "Twitchauth.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <tchar.h>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

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

// Twitch instance
TwitchAuth twitchAuth;

// Icon textures for connection status
ID3D11ShaderResourceView* g_iconNoNetwork = nullptr;
ID3D11ShaderResourceView* g_iconConnected = nullptr;

// UI State
static int selectedVoice = 0;
static const char* voiceNames[] = { "Paul", "Betty", "Harry", "Frank", "Dennis", "Kit", "Ursula", "Rita", "Wendy" };
static int volume = 100;
static char textBuffer[1024] = "Hello, I am DECtalk.";

// Window state
static bool isMaximized = false;
static RECT restoreRect = {};

// Win98 Color Scheme
void ApplyWin98Style()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Classic Win98 colors
    colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f); // Win98 gray
    colors[ImGuiCol_ChildBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.0f);
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
    colors[ImGuiCol_Header] = ImVec4(0.0f, 0.0f, 0.5f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.0f, 0.6f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.0f, 0.7f, 1.0f);
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
    style.FramePadding = ImVec2(4.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ScrollbarSize = 16.0f;
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
        const char* menuItems[] = { "File", "Edit", "About" };
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

            if (isClicked) {
                // TODO: Open menu dropdown
            }

            menuX += menuItemWidth + 1;
        }

        // STEP 4: Tab Bar (Win98 paper-tab style)
        static int selectedTab = 0;
        const char* tabNames[] = { "Voice Settings", "Connections" };
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
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Text to Speak");
            ImGui::InputTextMultiline("##text", textBuffer, IM_ARRAYSIZE(textBuffer), ImVec2(-1, 100));

            ImGui::Spacing();

            if (Win98Button("Speak", ImVec2(200, 30)))
            {
                dectalk.Speak(textBuffer);
            }
        }
        else if (selectedTab == 1) {
            // Connections tab content
            ImGui::Text("Twitch Connection");
            ImGui::Separator();
            ImGui::Spacing();

            // Connect button
            ConnectionStatus currentStatus = twitchAuth.GetStatus();
            bool isConnected = (currentStatus == ConnectionStatus::Connected);
            bool isConnecting = (currentStatus == ConnectionStatus::Connecting);

            const char* buttonText = isConnected ? "Connected to Twitch" :
                isConnecting ? "Connecting..." :
                "Connect To Twitch";

            if (Win98Button(buttonText, ImVec2(200, 30)))
            {
                if (!twitchAuth.IsAuthenticated()) {
                    // Not authenticated - start OAuth flow (will auto-connect after)
                    twitchAuth.StartAuthentication();
                }
                else if (!isConnected && !isConnecting) {
                    // Authenticated but not connected - reconnect to user's channel
                    twitchAuth.Connect(twitchAuth.GetUsername());
                }
            }

            ImGui::Spacing();

            // Status display with icon
            ImGui::Text("Status - ");
            ImGui::SameLine();

            // Display connection status icon
            ConnectionStatus status = twitchAuth.GetStatus();
            ID3D11ShaderResourceView* statusIcon = nullptr;

            if (status == ConnectionStatus::Connected) {
                statusIcon = g_iconConnected;
            }
            else {
                statusIcon = g_iconNoNetwork;
            }

            // Display icon if loaded
            if (statusIcon != nullptr) {
                ImGui::Image((void*)statusIcon, ImVec2(32, 32));
            }
            else {
                // Fallback to text if icons aren't loaded
                if (status == ConnectionStatus::Connected) {
                    ImGui::Text("[CONNECTED]");
                }
                else {
                    ImGui::Text("[NOT CONNECTED]");
                }
            }

            ImGui::SameLine();
            ImGui::Text("%s", twitchAuth.GetStatusMessage().c_str());
        }

        ImGui::EndChild(); // TabContent
        ImGui::PopStyleColor(); // Pop white background

        ImGui::End();
        ImGui::PopStyleVar(3);

        ImGui::PopFont(); // Pop regular font

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.76f, 0.76f, 0.76f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

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