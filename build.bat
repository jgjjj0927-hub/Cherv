@echo off
setlocal

echo [*] building driver
cd drv
call build.bat
if errorlevel 1 exit /b 1
cd ..

echo [*] building bin2h
g++ -O2 bin2h.cpp -o bin2h.exe -static
if errorlevel 1 exit /b 1

echo [*] generating driver_blob.h
bin2h.exe drv\NvRmDrv.sys driver_blob.h
if errorlevel 1 exit /b 1

echo [*] building svchost.exe (single exe)
g++ -O2 -municode -mwindows -static -static-libgcc -static-libstdc++ ^
    main.cpp -o svchost.exe ^
    -lnetapi32 -lws2_32 -ladvapi32 -luser32 -lgdi32 -lole32 -lshell32
if errorlevel 1 exit /b 1

echo [+] done: svchost.exe
endlocal
