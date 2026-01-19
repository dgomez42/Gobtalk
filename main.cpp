#define NOMINMAX
#include "Win95Style.h"
#include "DECtalkWrapper.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

// DirectX data
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

// DECtalk instance
DECtalkWrapper* g_tts = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

    // Allocate a console for debugging
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "GobTalk Debug Console" << std::endl;

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"GobTalk", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"GobTalk - DECtalk Twitch Bot",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);

    SetWindowTheme(hwnd, L"", L"");

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
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// Applies styling from Win95Style.h
    ApplyWin98Style();

    // Initialize DECtalk
    g_tts = new DECtalkWrapper();
    if (!g_tts->Initialize("DECtalk.dll")) {
        MessageBoxA(hwnd, "Failed to initialize DECtalk!\n\nMake sure DECtalk.dll and dtalk_us.dic are in the same folder as the executable.", "Error", MB_OK | MB_ICONERROR);
    }

    // Our state
    ImVec4 clear_color = ImVec4(0.75f, 0.75f, 0.75f, 1.00f); // Win95 gray
    char testMessage[256] = "Hello, this is a test message!";
    int currentVoice = 0;
    int volume = 100;

    // Main loop
    bool done = false;
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

        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("GobTalk - DECtalk Twitch Bot", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_MenuBar);

        // Manual menu bar
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));

        if (ImGui::Button("File")) ImGui::OpenPopup("FileMenu");
        ImGui::SameLine();
        if (ImGui::Button("Edit")) ImGui::OpenPopup("EditMenu");
        ImGui::SameLine();
        if (ImGui::Button("Help")) ImGui::OpenPopup("HelpMenu");

        // File menu popup
        if (ImGui::BeginPopup("FileMenu")) {
            if (ImGui::MenuItem("Exit")) {
                done = true;
            }
            ImGui::EndPopup();
        }

        // Edit menu popup
        if (ImGui::BeginPopup("EditMenu")) {
            if (ImGui::MenuItem("Preferences")) {
                // TODO
            }
            ImGui::EndPopup();
        }

        // Help menu popup
        if (ImGui::BeginPopup("HelpMenu")) {
            if (ImGui::MenuItem("About")) {
                // TODO
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImGui::Separator(); // Line under menu bar
        
        // Content area with padding
        ImGui::SetCursorPos(ImVec2(8, ImGui::GetCursorPosY() + 4));
        ImGui::BeginChild("ContentArea", ImVec2(io.DisplaySize.x - 16, io.DisplaySize.y - ImGui::GetCursorPosY() - 8), false);

        // Tab bar
        static int activeTab = 0;
        ImGui::BeginGroup();

        if (Win98Button("Voice Settings", ImVec2(120, 0))) activeTab = 0;
        ImGui::SameLine(0, 2);
        if (Win98Button("Connection", ImVec2(120, 0))) activeTab = 1;
        ImGui::SameLine(0, 2);
        if (Win98Button("Permissions", ImVec2(120, 0))) activeTab = 2;
        ImGui::SameLine(0, 2);
        if (Win98Button("Filters", ImVec2(120, 0))) activeTab = 3;

        ImGui::EndGroup();

        // Draw line under tabs
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 line_start = ImGui::GetItemRectMin();
        line_start.y = ImGui::GetItemRectMax().y;
        ImVec2 line_end = ImVec2(io.DisplaySize.x - 16, line_start.y);
        draw_list->AddLine(line_start, line_end, WIN98_DARK_GRAY);
        draw_list->AddLine(ImVec2(line_start.x, line_start.y + 1), ImVec2(line_end.x, line_end.y + 1), WIN98_WHITE);

        ImGui::Spacing();
        ImGui::Spacing();

        // Tab content
        if (activeTab == 0) {
            // Voice Settings
            BeginWin98GroupBox("Voice Selection", ImVec2(400, 0));

            const char* voices[] = { "Paul", "Betty", "Harry", "Frank", "Dennis", "Kit", "Ursula", "Rita", "Wendy" };
            ImGui::Text("Current Voice:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("##voice", &currentVoice, voices, IM_ARRAYSIZE(voices))) {
                if (g_tts) g_tts->SetVoice(currentVoice);
            }

            EndWin98GroupBox("Voice Selection");

            ImGui::Spacing();

            BeginWin98GroupBox("Volume", ImVec2(400, 0));

            ImGui::Text("Master Volume:");
            ImGui::SetNextItemWidth(250);
            if (ImGui::SliderInt("##volume", &volume, 0, 100, "%d%%")) {
                if (g_tts) g_tts->SetVolume(volume);
            }

            EndWin98GroupBox("Volume");

            ImGui::Spacing();

            BeginWin98GroupBox("Test Voice", ImVec2(400, 0));

            ImGui::Text("Test Message:");
            ImGui::SetNextItemWidth(350);
            ImGui::InputText("##testmsg", testMessage, IM_ARRAYSIZE(testMessage));

            ImGui::Spacing();
            if (Win98Button("Speak Test Message", ImVec2(140, 0))) {
                if (g_tts) {
                    g_tts->Speak(testMessage);
                }
            }

            EndWin98GroupBox("Test Voice");
        }
        else if (activeTab == 1) {
            // Connection
            BeginWin98GroupBox("Twitch", ImVec2(400, 0));

            ImGui::Text("Status: Not Connected");
            ImGui::Spacing();

            if (Win98Button("Connect to Twitch", ImVec2(140, 0))) {
                // TODO: Implement connection
            }

            EndWin98GroupBox("Twitch");

            ImGui::Spacing();

            BeginWin98GroupBox("YouTube", ImVec2(400, 0));

            ImGui::Text("Status: Not Connected");
            ImGui::Spacing();

            if (Win98Button("Connect to YouTube", ImVec2(140, 0))) {
                // TODO: Implement connection
            }

            EndWin98GroupBox("YouTube");
        }
        else if (activeTab == 2) {
            // Permissions
            BeginWin98GroupBox("Enable TTS For", ImVec2(400, 0));

            static bool chatEnabled = false;
            static bool subsEnabled = true;
            static bool bitsEnabled = true;
            static bool channelPointsEnabled = true;
            static bool vipEnabled = true;
            static bool modEnabled = true;

            ImGui::Checkbox("Chat Messages", &chatEnabled);
            ImGui::Checkbox("Subscriptions", &subsEnabled);
            ImGui::Checkbox("Bit Donations", &bitsEnabled);
            ImGui::Checkbox("Channel Point Redeems", &channelPointsEnabled);
            ImGui::Checkbox("VIPs", &vipEnabled);
            ImGui::Checkbox("Moderators", &modEnabled);

            EndWin98GroupBox("Enable TTS For");
        }
        else if (activeTab == 3) {
            // Filters
            BeginWin98GroupBox("Message Filtering", ImVec2(400, 0));

            ImGui::Text("Configure word and user filters");
            ImGui::Spacing();

            if (Win98Button("Edit Blacklist", ImVec2(140, 0))) {
                // TODO: Open blacklist editor
            }
            ImGui::SameLine();
            if (Win98Button("Edit Whitelist", ImVec2(140, 0))) {
                // TODO: Open whitelist editor
            }

            EndWin98GroupBox("Message Filtering");
        }

        ImGui::EndChild();
        ImGui::End();

        // Rendering
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    delete g_tts;
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
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LRESULT(lParam) & 0xFFFF, (UINT)LRESULT(lParam) >> 16, DXGI_FORMAT_UNKNOWN, 0);
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