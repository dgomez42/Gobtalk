#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "ttsapi.h"

class DECtalkWrapper {
private:
    HMODULE hDll;
    LPTTS_HANDLE_T ttsHandle;

    // Function pointers
    typedef MMRESULT(WINAPI* pfnStartup)(HWND, LPTTS_HANDLE_T*, UINT, DWORD);
    typedef MMRESULT(WINAPI* pfnSpeak)(LPTTS_HANDLE_T, LPSTR, DWORD);
    typedef MMRESULT(WINAPI* pfnShutdown)(LPTTS_HANDLE_T);
    typedef MMRESULT(WINAPI* pfnSetSpeaker)(LPTTS_HANDLE_T, SPEAKER_T);
    typedef MMRESULT(WINAPI* pfnSetVolume)(LPTTS_HANDLE_T, int);

    pfnStartup pStartup;
    pfnSpeak pSpeak;
    pfnShutdown pShutdown;
    pfnSetSpeaker pSetSpeaker;
    pfnSetVolume pSetVolume;

    int currentVoice;
    int currentVolume;

public:
    DECtalkWrapper();
    ~DECtalkWrapper();

    bool Initialize(const std::string& dllPath, int audioDevice = WAVE_MAPPER);
    bool Speak(const std::string& text);
    bool SetVoice(int voiceIndex); // 0-8: Paul, Betty, Harry, Frank, Dennis, Kit, Ursula, Rita, Wendy
    bool SetVolume(int volume); // 0-100
    std::vector<std::string> GetVoiceNames();
};