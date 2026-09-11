@echo off
setlocal
cd /d "%~dp0"

echo Compilando matrices.c...
set "COMPILADOR=gcc"
where gcc >nul 2>nul
if errorlevel 1 if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "COMPILADOR=C:\msys64\mingw64\bin\gcc.exe"
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)

"%COMPILADOR%" -std=c11 -O2 -Wall -Wextra -Wpedantic matrices.c -o matrices.exe

if errorlevel 1 (
    echo.
    echo No se pudo compilar. Verifica que gcc este disponible en el PATH.
    pause
    exit /b 1
)

echo Compilacion terminada correctamente.
echo.
matrices.exe

echo.
pause
