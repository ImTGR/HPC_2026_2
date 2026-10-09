@echo off
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ejecutar_pruebas_O3.ps1"
if errorlevel 1 (
    echo.
    echo Las pruebas no terminaron correctamente.
    pause
    exit /b 1
)

echo.
echo El archivo resultados_matrices_O3.csv ya esta listo para abrirlo con Excel.
pause