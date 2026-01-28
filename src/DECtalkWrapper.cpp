#define NOMINMAX
#include "DECtalkWrapper.h"
#include <iostream>
#include <algorithm>

DECtalkWrapper::DECtalkWrapper()
    : hDll(nullptr), ttsHandle(nullptr), currentVoice(0), currentVolume(100) {
}

DECtalkWrapper::~DECtalkWrapper() {
    if (ttsHandle && pShutdown) {
        pShutdown(ttsHandle);
    }
    if (hDll) {
        FreeLibrary(hDll);
    }
}

bool DECtalkWrapper::Initialize(const std::string& dllPath, int audioDevice) {

    // Load DLL
    hDll = LoadLibraryA(dllPath.c_str());
    if (!hDll) {
        std::cerr << "Failed to load DECtalk.dll from: " << dllPath << std::endl;
        return false;
    }

    // Get function pointers
    pStartup = (pfnStartup)GetProcAddress(hDll, "TextToSpeechStartup");
    pSpeak = (pfnSpeak)GetProcAddress(hDll, "TextToSpeechSpeak");
    pShutdown = (pfnShutdown)GetProcAddress(hDll, "TextToSpeechShutdown");
    pSetSpeaker = (pfnSetSpeaker)GetProcAddress(hDll, "TextToSpeechSetSpeaker");
    pSetVolume = (pfnSetVolume)GetProcAddress(hDll, "TextToSpeechSetVolume");
    pReset = (pfnReset)GetProcAddress(hDll, "TextToSpeechReset");
    pSync = (pfnSync)GetProcAddress(hDll, "TextToSpeechSync");

    if (!pStartup || !pSpeak || !pShutdown) {
        std::cerr << "Failed to get DECtalk functions" << std::endl;
        return false;
    }

    // Initialize TTS engine
    MMRESULT result = pStartup(nullptr, &ttsHandle, audioDevice, OWN_AUDIO_DEVICE);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to initialize TTS engine. Error: " << result << std::endl;
        return false;
    }

    // Set initial voice and volume
    SetVoice(0);
    SetVolume(100);

    std::cout << "DECtalk initialized successfully" << std::endl;
    return true;
}

bool DECtalkWrapper::Speak(const std::string& text) {
    if (!SpeakAsync(text)) return false;
    return WaitForCompletion();
}

bool DECtalkWrapper::SpeakAsync(const std::string& text) {
    if (!ttsHandle || !pSpeak) return false;

    // Build volume command and combine with text
    std::string volumeCmd = "[:volume set " + std::to_string(currentVolume) + "]";
    std::string fullText = volumeCmd + text;

    std::cout << "Speaking (async) with volume command: " << volumeCmd << std::endl;

    // Use 0 instead of TTS_FORCE for asynchronous speech
    MMRESULT result = pSpeak(ttsHandle, &fullText[0], 0);
    return (result == MMSYSERR_NOERROR);
}

bool DECtalkWrapper::WaitForCompletion() {
    if (!ttsHandle || !pSync) {
        std::cerr << "WaitForCompletion failed: Invalid handle or function pointer" << std::endl;
        return false;
    }

    // TextToSpeechSync waits until all queued speech is complete
    MMRESULT result = pSync(ttsHandle);
    return (result == MMSYSERR_NOERROR);
}

bool DECtalkWrapper::SetVoice(int voiceIndex) {
    if (!ttsHandle || !pSetSpeaker) return false;

    // Clamp to valid range
    voiceIndex = std::max(0, std::min(8, voiceIndex));
    currentVoice = voiceIndex;

    MMRESULT result = pSetSpeaker(ttsHandle, (SPEAKER_T)voiceIndex);
    return (result == MMSYSERR_NOERROR);
}

bool DECtalkWrapper::SetVolume(int volume) {
    if (!ttsHandle) {
        std::cerr << "SetVolume failed: No TTS handle" << std::endl;
        return false;
    }

    if (!pSetVolume) {
        std::cerr << "SetVolume failed: Function pointer is null" << std::endl;
        return false;
    }

    // Updates volume to match slider via embedded text command
    volume = (std::max)(0, (std::min)(100, volume));
    currentVolume = volume;

    std::cout << "Setting volume to: " << volume << std::endl;
    MMRESULT result = pSetVolume(ttsHandle, volume);
    std::cout << "SetVolume result: " << result << std::endl;

    return (result == MMSYSERR_NOERROR);
}

std::vector<std::string> DECtalkWrapper::GetVoiceNames() {
    return {
        "Paul", "Betty", "Harry", "Frank", "Dennis",
        "Kit", "Ursula", "Rita", "Wendy"
    };
}

void DECtalkWrapper::Stop() {
    if (!ttsHandle || !pReset) {
        std::cerr << "Stop failed: Invalid handle or function pointer" << std::endl;
        return;
    }

    // Reset with TRUE clears the audio queue completely
    MMRESULT result = pReset(ttsHandle, TRUE);
    if (result == MMSYSERR_NOERROR) {
        std::cout << "[DECtalk] Queue cleared (Stop)" << std::endl;
    }
    else {
        std::cerr << "[DECtalk] Stop failed with error: " << result << std::endl;
    }
}

void DECtalkWrapper::Skip() {
    if (!ttsHandle || !pReset) {
        std::cerr << "Skip failed: Invalid handle or function pointer" << std::endl;
        return;
    }

    // Reset with FALSE stops current message
    // This causes WaitForCompletion() to return, allowing ProcessLoop to continue to next message
    MMRESULT result = pReset(ttsHandle, FALSE);
    if (result == MMSYSERR_NOERROR) {
        std::cout << "[DECtalk] Skipped current message" << std::endl;
    }
    else {
        std::cerr << "[DECtalk] Skip failed with error: " << result << std::endl;
    }
}