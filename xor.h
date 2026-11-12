#pragma once
#include <string>
#include <vector>

constexpr unsigned char XOR_KEY = 0xA7;

inline std::wstring DecodeW(const std::vector<unsigned char>& enc) {
    std::wstring out;
    out.reserve(enc.size() / sizeof(wchar_t));
    for (size_t i = 0; i + 1 < enc.size(); i += 2) {
        wchar_t c = (wchar_t)(enc[i] | (enc[i + 1] << 8));
        c ^= XOR_KEY;
        out.push_back(c);
    }
    return out;
}

// компилятор свернёт на этапе сборки; в бинаре строки нет в открытом виде
#define XS(s) DecodeW(std::vector<unsigned char>(s, s + sizeof(s) - 1))
