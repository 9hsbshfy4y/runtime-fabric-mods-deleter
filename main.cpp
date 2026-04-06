#include <windows.h>
#include <string>
#include <io.h>
#include <fcntl.h>

#include "utils/nt_types.h"
#include "utils/privileges.h"
#include "utils/handle_finder.h"

static std::wstring Trim(std::wstring s) {
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' '))
        s.pop_back();
    while (!s.empty() && s.front() == L' ')
        s.erase(s.begin());
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"')
        s = s.substr(1, s.size() - 2);
    return s;
}

static std::wstring ExtractFileName(const std::wstring &path) {
    auto pos = path.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? path.substr(pos + 1) : path;
}

int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    wprintf(L"@jniadmin\n\n");

    if (!LoadNtFunctions()) {
        wprintf(L"[!] Failed to load ntdll functions\n");
        return 1;
    }

    if (!EnableDebugPrivilege()) {
        wprintf(L"[!] SeDebugPrivilege failed, run as Administrator\n");
        return 1;
    }

    wprintf(L"File path or name:\n> ");
    fflush(stdout);

    wchar_t buf[1024]{};
    if (!fgetws(buf, 1024, stdin))
        return 1;

    std::wstring input = Trim(buf);
    if (input.empty()) {
        wprintf(L"[!] No path entered\n");
        return 1;
    }

    std::wstring target = ExtractFileName(input);
    wprintf(L"\n[*] Target: \"%ls\" (java/javaw only)\n\n", target.c_str());

    auto handles = FindFileHandles(target);

    if (handles.empty()) {
        wprintf(L"\n[*] Nothing found\n");
        return 0;
    }

    wprintf(L"[*] Found %zu handle(s), closing...\n\n", handles.size());

    int closed = 0;
    for (const auto &h : handles) {
        wprintf(L"  PID=%-6lu %-14ls ", h.pid, h.processName.c_str());

        bool ok = false;
        if (h.kind == HandleKind::FileHandle) {
            wprintf(L"handle=0x%04X ", h.handleValue);
            ok = CloseRemoteHandle(h.pid, h.handleValue);
        } else {
            wprintf(L"base=%p ", h.moduleBase);
            ok = UnloadRemoteModule(h.pid, h.moduleBase);
        }

        wprintf(ok ? L"-> OK\n" : L"-> FAIL\n");
        if (ok) ++closed;
    }

    wprintf(L"\n[*] Closed %d / %zu\n", closed, handles.size());
    if (closed > 0)
        wprintf(L"[+] File should now be deletable\n");

    wprintf(L"\n[~] Press Enter to exit...");
    fflush(stdout);
    getwchar();

    return 0;
}
