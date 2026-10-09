@echo off
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ejecutar_pruebas_secuencial.ps1"
if errorlevel 1 (
    echo.
    echo Las pruebas no terminaron correctamente.
    pause
    exit /b 1
)

echo.
echo El archivo resultados_matrices_secuencial.csv ya esta listo para abrirlo con Excel.
pause
