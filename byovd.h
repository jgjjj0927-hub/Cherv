#pragma once
#include <windows.h>
#include <string>

class Iqvw {
public:
    HANDLE h = INVALID_HANDLE_VALUE;
    bool Open() {
        h = CreateFileW(L"\\\\.\\Nal", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) h = CreateFileW(L"\\\\.\\iqvw64e", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        return h != INVALID_HANDLE_VALUE;
    }
    bool WriteMsr(DWORD reg, DWORD eax, DWORD edx) {
        if (h == INVALID_HANDLE_VALUE) return false;
        struct { DWORD reg; DWORD eax; DWORD edx; } in = { reg, eax, edx };
        DWORD ret = 0;
        return DeviceIoControl(h, 0x80862007, &in, sizeof(in), &ret, sizeof(ret), &ret, nullptr) != 0;
    }
    bool OutPort(DWORD port, DWORD val) {
        if (h == INVALID_HANDLE_VALUE) return false;
        struct { DWORD port; DWORD val; } in = { port, val };
        DWORD ret = 0;
        return DeviceIoControl(h, 0x80862008, &in, sizeof(in), &ret, sizeof(ret), &ret, nullptr) != 0;
    }
    void Close() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};

class Gdrv {
public:
    HANDLE h = INVALID_HANDLE_VALUE;
    bool Open() {
        h = CreateFileW(L"\\\\.\\gdrv", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        return h != INVALID_HANDLE_VALUE;
    }
    bool WriteMsr(DWORD reg, DWORD eax, DWORD edx) {
        if (h == INVALID_HANDLE_VALUE) return false;
        struct { DWORD reg; DWORD eax; DWORD edx; } in = { reg, eax, edx };
        DWORD ret = 0;
        return DeviceIoControl(h, 0xC3502004, &in, sizeof(in), &ret, sizeof(ret), &ret, nullptr) != 0;
    }
    bool OutPort(DWORD port, DWORD val) {
        if (h == INVALID_HANDLE_VALUE) return false;
        struct { DWORD port; DWORD val; } in = { port, val };
        DWORD ret = 0;
        return DeviceIoControl(h, 0xC3502808, &in, sizeof(in), &ret, sizeof(ret), &ret, nullptr) != 0;
    }
    void Close() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};

bool InstallByovd(const std::wstring& sysName, const std::wstring& svcName) {
    std::wstring dst = L"C:\\Windows\\System32\\drivers\\" + sysName;
    if (GetFileAttributesW(sysName.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    CopyFileW(sysName.c_str(), dst.c_str(), FALSE);
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;
    SC_HANDLE svc = CreateServiceW(scm, svcName.c_str(), svcName.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE, dst.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!svc) svc = OpenServiceW(scm, svcName.c_str(), SERVICE_ALL_ACCESS);
    if (!svc) { CloseServiceHandle(scm); return false; }
    BOOL started = StartServiceW(svc, 0, nullptr);
    DWORD err = started ? 0 : GetLastError();
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return started || err == ERROR_SERVICE_ALREADY_RUNNING;
}
