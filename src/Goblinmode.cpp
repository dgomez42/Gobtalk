#define NOMINMAX
#include "GoblinMode.h"
#include <iostream>
#include <algorithm>

GoblinMode::GoblinMode()
    : hDll(nullptr), currentVoice(0), currentVolume(100) {
}

GoblinMode::~GoblinMode() {
    StopAllChaos();

    if (hDll) {
        FreeLibrary(hDll);
    }
}

bool GoblinMode::Initialize(const std::string& dllPath, int audioDevice) {
    this->dllPath = dllPath;

    // Load DLL
    hDll = LoadLibraryA(dllPath.c_str());
    if (!hDll) {
        std::cerr << "[GoblinMode] Failed to load DECtalk.dll from: " << dllPath << std::endl;
        return false;
    }

    // Get function pointers
    pStartup = (pfnStartup)GetProcAddress(hDll, "TextToSpeechStartup");
    pSpeak = (pfnSpeak)GetProcAddress(hDll, "TextToSpeechSpeak");
    pShutdown = (pfnShutdown)GetProcAddress(hDll, "TextToSpeechShutdown");
    pSetSpeaker = (pfnSetSpeaker)GetProcAddress(hDll, "TextToSpeechSetSpeaker");
    pSetVolume = (pfnSetVolume)GetProcAddress(hDll, "TextToSpeechSetVolume");
    pSync = (pfnSync)GetProcAddress(hDll, "TextToSpeechSync");

    if (!pStartup || !pSpeak || !pShutdown || !pSetSpeaker || !pSetVolume || !pSync) {
        std::cerr << "[GoblinMode] Failed to load required DECtalk functions" << std::endl;
        FreeLibrary(hDll);
        hDll = nullptr;
        return false;
    }

    std::cout << "[GoblinMode] Initialized! Ready for CHAOS!" << std::endl;
    return true;
}

LPTTS_HANDLE_T GoblinMode::CreateTemporaryHandle() {
    if (!pStartup) return nullptr;

    LPTTS_HANDLE_T tempHandle = nullptr;
    MMRESULT result = pStartup(nullptr, &tempHandle, WAVE_MAPPER, 0);

    if (result != MMSYSERR_NOERROR || !tempHandle) {
        std::cerr << "[GoblinMode] Failed to create temporary handle" << std::endl;
        return nullptr;
    }

    // Set voice and volume for this handle
    if (pSetSpeaker) {
        pSetSpeaker(tempHandle, currentVoice);
    }
    if (pSetVolume) {
        pSetVolume(tempHandle, currentVolume);
    }

    return tempHandle;
}

void GoblinMode::SpeakChaos(const std::string& text) {
    if (!hDll) {
        std::cerr << "[GoblinMode] Not initialized!" << std::endl;
        return;
    }

    // Clean up old finished threads
    CleanupFinishedThreads();

    std::cout << "[GoblinMode] *** SPAWNING CHAOS THREAD ***" << std::endl;

    // Spawn a new thread with its OWN DECtalk handle
    std::thread chaosThread([this, text]() {
        // Create a NEW handle for THIS thread
        LPTTS_HANDLE_T myHandle = CreateTemporaryHandle();

        if (!myHandle) {
            std::cerr << "[GoblinMode] Thread failed to create handle!" << std::endl;
            return;
        }

        std::cout << "[GoblinMode] Thread speaking: " << text.substr(0, 30) << "..." << std::endl;

        // Build volume command
        std::string volumeCmd = "[:volume set " + std::to_string(currentVolume) + "]";
        std::string fullText = volumeCmd + text;

        // SPEAK with this thread's handle (async)
        if (pSpeak) {
            pSpeak(myHandle, &fullText[0], 0);  // 0 = async
        }

        // Wait for speech to actually complete using DECtalk's Sync function
        // This ensures the entire message plays before we shut down the handle
        if (pSync) {
            pSync(myHandle);  // Blocks until speech finishes
        }

        // Shutdown this thread's handle
        if (pShutdown) {
            pShutdown(myHandle);
        }

        std::cout << "[GoblinMode] Thread finished!" << std::endl;
        });

    // Store thread for tracking
    {
        std::lock_guard<std::mutex> lock(threadsMutex);
        activeThreads.push_back(std::move(chaosThread));
    }

    std::cout << "[GoblinMode] Total active threads: " << GetActiveThreadCount() << std::endl;
}

void GoblinMode::CleanupFinishedThreads() {
    std::lock_guard<std::mutex> lock(threadsMutex);

    // Remove finished threads
    activeThreads.erase(
        std::remove_if(activeThreads.begin(), activeThreads.end(),
            [](std::thread& t) {
                if (t.joinable()) {
                    // Try to join (won't block if thread is done)
                    return false;
                }
                return true;
            }),
        activeThreads.end()
    );
}

void GoblinMode::SetVoice(int voiceIndex) {
    currentVoice = voiceIndex;
    std::cout << "[GoblinMode] Voice set to: " << voiceIndex << std::endl;
}

void GoblinMode::SetVolume(int volume) {
    // Manual clamp since std::clamp requires C++17
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    currentVolume = volume;
    std::cout << "[GoblinMode] Volume set to: " << currentVolume << std::endl;
}

void GoblinMode::StopAllChaos() {
    std::cout << "[GoblinMode] STOPPING ALL CHAOS..." << std::endl;

    std::lock_guard<std::mutex> lock(threadsMutex);

    // Detach all threads (let them finish naturally)
    for (auto& thread : activeThreads) {
        if (thread.joinable()) {
            thread.detach();
        }
    }

    activeThreads.clear();
    std::cout << "[GoblinMode] All chaos stopped!" << std::endl;
}

int GoblinMode::GetActiveThreadCount() {
    std::lock_guard<std::mutex> lock(threadsMutex);
    return (int)activeThreads.size();
}