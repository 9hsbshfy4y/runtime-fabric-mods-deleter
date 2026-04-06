#pragma once
#include <windows.h>
#include <string>
#include <vector>

enum class HandleKind {
    FileHandle,
    DllModule
};

struct FoundHandle {
    DWORD pid;
    USHORT handleValue;
    HMODULE moduleBase;
    HandleKind kind;
    std::wstring processName;
    std::wstring path;
};

std::vector<FoundHandle> FindFileHandles(const std::wstring &targetFileName);
bool CloseRemoteHandle(DWORD pid, USHORT handleValue);
bool UnloadRemoteModule(DWORD pid, HMODULE moduleBase);
