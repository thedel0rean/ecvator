#include <memory>
#include <clocale>
#include <Windows.h>
#include "AntiDetection.h"
#include "Hooks.h"
//#include <string>
#include <iostream>
#include <mmsystem.h>
#include "xor.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

inline bool exist_chk(const std::string& name) 
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

extern "C" BOOL WINAPI _CRT_INIT(HMODULE moduleHandle, DWORD reason, LPVOID reserved);
BOOL APIENTRY DllEntryPoint(HMODULE moduleHandle, DWORD reason, LPVOID reserved)
{
    if (!_CRT_INIT(moduleHandle, reason, reserved))
        return FALSE;

    if (reason == DLL_PROCESS_ATTACH)
    {
        // AntiDetection::AntiDetection();

        std::setlocale(LC_CTYPE, ".utf8");
        //PlaySoundA(skCrypt("C:\\Windows\\Media\\notify"), NULL, SND_SYNC);
        hooks = std::make_unique<Hooks>(moduleHandle);
    }

    return TRUE;
}
