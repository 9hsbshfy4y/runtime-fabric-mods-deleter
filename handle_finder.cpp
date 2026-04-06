#include "utils/handle_finder.h"
#include "utils/nt_types.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#pragma comment(lib, "psapi.lib")

pNtQuerySystemInformation fnNtQuerySystemInformation = nullptr;
pNtQueryObject fnNtQueryObject = nullptr;
pNtDuplicateObject fnNtDuplicateObject = nullptr;

bool LoadNtFunctions() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;

    fnNtQuerySystemInformation = reinterpret_cast<pNtQuerySystemInformation>(
        GetProcAddress(ntdll, "NtQuerySystemInformation"));
    fnNtQueryObject = reinterpret_cast<pNtQueryObject>(
        GetProcAddress(ntdll, "NtQueryObject"));
    fnNtDuplicateObject = reinterpret_cast<pNtDuplicateObject>(
        GetProcAddress(ntdll, "NtDuplicateObject"));

    return fnNtQuerySystemInformation && fnNtQueryObject && fnNtDuplicateObject;
}

static std::wstring ToLower(std::wstring s) {
    std::ranges::transform(s, s.begin(), ::towlower);
    return s;
}

static std::unordered_map<DWORD, std::wstring> BuildProcessMap() {
    std::unordered_map<DWORD, std::wstring> map;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return map;

    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(snap, &pe)) {
        do {
            map[pe.th32ProcessID] = ToLower(pe.szExeFile);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return map;
}

static std::unordered_set<DWORD> FindJavaPids(
    const std::unordered_map<DWORD, std::wstring> &procs)
{
    std::unordered_set<DWORD> pids;
    for (const auto &[pid, name] : procs) {
        if (name == L"java.exe" || name == L"javaw.exe")
            pids.insert(pid);
    }
    return pids;
}

struct NameQuery {
    HANDLE handle;
    std::wstring name;
    bool ok = false;
};

static DWORD WINAPI QueryNameThread(LPVOID param) {
    auto *q = static_cast<NameQuery *>(param);

    ULONG size = 2048;
    auto *buf = new BYTE[size];
    ULONG ret = 0;

    NTSTATUS st = fnNtQueryObject(q->handle, MY_ObjectNameInformation, buf, size, &ret);
    if (st == static_cast<NTSTATUS>(0xC0000023L) || ret > size) {
        delete[] buf;
        size = ret + 64;
        buf = new BYTE[size];
        st = fnNtQueryObject(q->handle, MY_ObjectNameInformation, buf, size, &ret);
    }

    if (st == STATUS_SUCCESS) {
        auto *us = reinterpret_cast<MY_UNICODE_STRING *>(buf);
        if (us->Buffer && us->Length > 0) {
            q->name.assign(us->Buffer, us->Length / sizeof(WCHAR));
            q->ok = true;
        }
    }

    delete[] buf;
    return 0;
}

static std::wstring GetHandleName(HANDLE dup) {
    NameQuery q{dup};

    HANDLE th = CreateThread(nullptr, 0, QueryNameThread, &q, 0, nullptr);
    if (!th) return {};

    if (WaitForSingleObject(th, 200) == WAIT_TIMEOUT) {
        TerminateThread(th, 0);
        CloseHandle(th);
        return {};
    }
    CloseHandle(th);
    return q.ok ? q.name : std::wstring{};
}

static void ScanFileHandles(
    const std::wstring &target,
    const std::unordered_set<DWORD> &javaPids,
    const std::unordered_map<DWORD, std::wstring> &procs,
    std::vector<FoundHandle> &out)
{
    ULONG bufSize = 4 * 1024 * 1024;
    BYTE *buffer = nullptr;
    NTSTATUS status;

    do {
        buffer = new BYTE[bufSize];
        status = fnNtQuerySystemInformation(MY_SystemHandleInformation, buffer, bufSize, nullptr);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            delete[] buffer;
            buffer = nullptr;
            bufSize *= 2;
            if (bufSize > 256 * 1024 * 1024) return;
        }
    } while (status == STATUS_INFO_LENGTH_MISMATCH);

    if (status != STATUS_SUCCESS) {
        delete[] buffer;
        return;
    }

    auto *info = reinterpret_cast<MY_SYSTEM_HANDLE_INFORMATION *>(buffer);
    DWORD myPid = GetCurrentProcessId();

    std::unordered_map<DWORD, HANDLE> cache;
    auto getProc = [&](DWORD pid) -> HANDLE {
        auto it = cache.find(pid);
        if (it != cache.end()) return it->second;
        HANDLE h = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        cache[pid] = h;
        return h;
    };

    for (ULONG i = 0; i < info->HandleCount; i++) {
        auto &e = info->Handles[i];
        DWORD pid = e.ProcessId;

        if (pid == myPid || !javaPids.contains(pid))
            continue;

        HANDLE hProc = getProc(pid);
        if (!hProc) continue;

        HANDLE dup = nullptr;
        NTSTATUS ds = fnNtDuplicateObject(
            hProc,
            reinterpret_cast<HANDLE>(static_cast<uintptr_t>(e.HandleValue)),
            GetCurrentProcess(), &dup,
            0, 0, DUPLICATE_SAME_ACCESS);

        if (ds != STATUS_SUCCESS || !dup) continue;

        std::wstring name = GetHandleName(dup);
        CloseHandle(dup);

        if (name.empty()) continue;

        if (ToLower(name).find(target) != std::wstring::npos) {
            FoundHandle fh;
            fh.pid = pid;
            fh.handleValue = e.HandleValue;
            fh.moduleBase = nullptr;
            fh.kind = HandleKind::FileHandle;
            fh.processName = procs.contains(pid) ? procs.at(pid) : L"unknown";
            fh.path = std::move(name);
            out.push_back(std::move(fh));
        }
    }

    for (auto &h : cache | std::views::values)
        if (h) CloseHandle(h);

    delete[] buffer;
}

static void ScanModules(
    const std::wstring &target,
    const std::unordered_set<DWORD> &javaPids,
    const std::unordered_map<DWORD, std::wstring> &procs,
    std::vector<FoundHandle> &out)
{
    for (DWORD pid : javaPids) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProc) continue;

        HMODULE mods[4096];
        DWORD needed = 0;
        if (!EnumProcessModulesEx(hProc, mods, sizeof(mods), &needed, LIST_MODULES_ALL)) {
            CloseHandle(hProc);
            continue;
        }

        DWORD count = needed / sizeof(HMODULE);
        for (DWORD m = 0; m < count; ++m) {
            wchar_t path[MAX_PATH]{};
            if (!GetModuleFileNameExW(hProc, mods[m], path, MAX_PATH)) continue;

            if (ToLower(path).find(target) != std::wstring::npos) {
                FoundHandle fh;
                fh.pid = pid;
                fh.handleValue = 0;
                fh.moduleBase = mods[m];
                fh.kind = HandleKind::DllModule;
                fh.processName = procs.contains(pid) ? procs.at(pid) : L"unknown";
                fh.path = path;
                out.push_back(std::move(fh));
            }
        }

        CloseHandle(hProc);
    }
}

std::vector<FoundHandle> FindFileHandles(const std::wstring &targetFileName) {
    std::vector<FoundHandle> results;
    std::wstring target = ToLower(targetFileName);

    auto procs = BuildProcessMap();
    auto javaPids = FindJavaPids(procs);

    if (javaPids.empty()) {
        wprintf(L"[!] No java/javaw processes found\n");
        return results;
    }

    wprintf(L"[*] Found %zu java process(es)\n", javaPids.size());

    wprintf(L"[*] Scanning handles...\n");
    ScanFileHandles(target, javaPids, procs, results);

    wprintf(L"[*] Scanning modules...\n");
    ScanModules(target, javaPids, procs, results);

    return results;
}

bool CloseRemoteHandle(DWORD pid, USHORT handleValue) {
    HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    if (!hProc) return false;

    HANDLE dummy = nullptr;
    BOOL ok = DuplicateHandle(
        hProc,
        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handleValue)),
        nullptr, &dummy,
        0, FALSE, DUPLICATE_CLOSE_SOURCE);

    if (dummy) CloseHandle(dummy);
    CloseHandle(hProc);
    return ok != FALSE;
}

bool UnloadRemoteModule(DWORD pid, HMODULE moduleBase) {
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
        FALSE, pid);
    if (!hProc) return false;

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    auto freeLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(k32, "FreeLibrary"));

    if (!freeLib) {
        CloseHandle(hProc);
        return false;
    }

    HANDLE th = CreateRemoteThread(hProc, nullptr, 0, freeLib, moduleBase, 0, nullptr);
    if (!th) {
        CloseHandle(hProc);
        return false;
    }

    DWORD wait = WaitForSingleObject(th, 5000);
    DWORD exitCode = 0;
    GetExitCodeThread(th, &exitCode);
    CloseHandle(th);
    CloseHandle(hProc);

    return (wait == WAIT_OBJECT_0) && (exitCode != 0);
}
