@echo off
setlocal
cd /d "%~dp0"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ejecutar_pruebas_procesos.ps1"

if errorlevel 1 (
    echo.
    echo La ejecucion termino con un error.
) else (
    echo.
    echo Las pruebas terminaron correctamente.
)

pause
