@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "COMPILADOR=gcc"
where gcc >nul 2>nul
if errorlevel 1 if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "COMPILADOR=C:\msys64\mingw64\bin\gcc.exe"
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)

echo Compilando matrices.c...
"%COMPILADOR%" -std=c11 -O2 -Wall -Wextra -Wpedantic matrices.c -o matrices.exe
if errorlevel 1 (
    echo.
    echo No se pudo compilar. Verifica que gcc este disponible en el PATH.
    pause
    exit /b 1
)

set "ARCHIVO_CSV=resultados_indices.csv"
set "ARCHIVO_SALIDA=%TEMP%\matrices_prueba_%RANDOM%.txt"

> "%ARCHIVO_CSV%" echo orden,limite,semilla,tiempo_segundos

echo.
echo Iniciando pruebas sin semilla explicita, con valores aleatorios entre 1 y 500.
echo Los ordenes son: 500, 1000, 2000 y 4000.
echo.

for %%O in (500 1000 2000 4000) do (
    echo ==================================================
    echo Ejecutando orden %%O...
    matrices.exe %%O 500 > "%ARCHIVO_SALIDA%" 2>&1
    if errorlevel 1 (
        echo Error al ejecutar la prueba con orden %%O.
        type "%ARCHIVO_SALIDA%"
        del /q "%ARCHIVO_SALIDA%" >nul 2>nul
        pause
        exit /b 1
    )

    type "%ARCHIVO_SALIDA%"
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$texto = Get-Content -Raw -LiteralPath '%ARCHIVO_SALIDA%'; $orden = [regex]::Match($texto, 'Orden: (\d+) x \d+').Groups[1].Value; $limite = [regex]::Match($texto, 'Valores aleatorios: 1\.\.(\d+)').Groups[1].Value; $semilla = [regex]::Match($texto, 'Semilla: (\d+)').Groups[1].Value; $tiempo = [regex]::Match($texto, 'Tiempo de multiplicacion: ([0-9.]+) segundos').Groups[1].Value; if ([string]::IsNullOrWhiteSpace($orden) -or [string]::IsNullOrWhiteSpace($tiempo)) { exit 1 }; Add-Content -LiteralPath '%ARCHIVO_CSV%' -Value ($orden + ',' + $limite + ',' + $semilla + ',' + $tiempo)"
    if errorlevel 1 (
        echo Error al extraer los resultados de la prueba con orden %%O.
        del /q "%ARCHIVO_SALIDA%" >nul 2>nul
        pause
        exit /b 1
    )
    echo.
)

del /q "%ARCHIVO_SALIDA%" >nul 2>nul
echo ==================================================
echo Pruebas terminadas.
echo Resultados guardados en: %CD%\%ARCHIVO_CSV%
echo.
type "%ARCHIVO_CSV%"
echo.
pause
