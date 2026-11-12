#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include "config.h"
#include "winring0_blob.h"

bool WriteDriverFile(const std::wstring& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)WinRing0_sys, WinRing0_sys_len);
    f.close();
    return true;
}

bool InstallService(const std::wstring& sysPath) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;

    SC_HANDLE svc = CreateServiceW(scm, L"WinRing0_1_2_0", L"WinRing0_1_2_0",
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
        sysPath.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) {
        DWORD e = GetLastError();
        if (e != ERROR_SERVICE_EXISTS) { CloseServiceHandle(scm); return false; }
        svc = OpenServiceW(scm, L"WinRing0_1_2_0", SERVICE_ALL_ACCESS);
        if (!svc) { CloseServiceHandle(scm); return false; }
    }

    BOOL started = StartServiceW(svc, 0, nullptr);
    DWORD startErr = started ? 0 : GetLastError();

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    if (!started && startErr != ERROR_SERVICE_ALREADY_RUNNING)
        return false;
    return true;
}

int wmain() {
    std::wstring tmp = L"C:\\Windows\\Temp\\" + std::wstring(DRIVER_NAME);
    if (!WriteDriverFile(tmp)) {
        std::wcerr << L"[!] cannot write driver file\n";
        return 1;
    }

    std::wstring sysPath = L"C:\\Windows\\System32\\drivers\\" + std::wstring(DRIVER_NAME);
    CopyFileW(tmp.c_str(), sysPath.c_str(), FALSE);

    if (!InstallService(sysPath)) {
        std::wcerr << L"[!] install service failed\n";
        return 1;
    }

    std::wcout << L"[+] WinRing0 loaded\n";
    return 0;
}
