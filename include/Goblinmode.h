#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include "ttsapi.h"

// GoblinMode - Experimental TTS chaos engine
// Attempts to play multiple messages simultaneously for maximum audio chaos
class GoblinMode {
private:
    // DECtalk DLL
    HMODULE hDll;

    // Function pointers (shared across all handles)
    typedef MMRESULT(WINAPI* pfnStartup)(HWND, LPTTS_HANDLE_T*, UINT, DWORD);
    typedef MMRESULT(WINAPI* pfnSpeak)(LPTTS_HANDLE_T, LPSTR, DWORD);
    typedef MMRESULT(WINAPI* pfnShutdown)(LPTTS_HANDLE_T);
    typedef MMRESULT(WINAPI* pfnSetSpeaker)(LPTTS_HANDLE_T, SPEAKER_T);
    typedef MMRESULT(WINAPI* pfnSetVolume)(LPTTS_HANDLE_T, int);
    typedef MMRESULT(WINAPI* pfnSync)(LPTTS_HANDLE_T);

    pfnStartup pStartup;
    pfnSpeak pSpeak;
    pfnShutdown pShutdown;
    pfnSetSpeaker pSetSpeaker;
    pfnSetVolume pSetVolume;
    pfnSync pSync;

    // Settings
    std::string dllPath;
    int currentVoice;
    int currentVolume;

    // Thread tracking
    std::vector<std::thread> activeThreads;
    std::mutex threadsMutex;

    // Create a new temporary DECtalk handle
    LPTTS_HANDLE_T CreateTemporaryHandle();

    // Cleanup old threads
    void CleanupFinishedThreads();

public:
    GoblinMode();
    ~GoblinMode();

    // Initialize the Goblin mode system
    bool Initialize(const std::string& dllPath, int audioDevice = WAVE_MAPPER);

    // Speak a message with MAXIMUM CHAOS (spawns new thread + new handle)
    void SpeakChaos(const std::string& text);

    // Set voice for future messages
    void SetVoice(int voiceIndex);

    // Set volume for future messages
    void SetVolume(int volume);

    // Stop all chaos (kill all threads)
    void StopAllChaos();

    // Get current stats
    int GetActiveThreadCount();
};