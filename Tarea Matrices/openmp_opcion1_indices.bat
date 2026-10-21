@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "COMPILADOR="
if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set "COMPILADOR=C:\msys64\ucrt64\bin\gcc.exe"
    set "PATH=C:\msys64\ucrt64\bin;%PATH%"
) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "COMPILADOR=C:\msys64\mingw64\bin\gcc.exe"
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)
if not defined COMPILADOR (
    where gcc >nul 2>nul
    if not errorlevel 1 set "COMPILADOR=gcc"
)
if not defined COMPILADOR (
    echo No se encontro GCC. Instala MSYS2 MinGW-w64 para compilar OpenMP en Windows.
    pause
    exit /b 1
)
for %%G in ("%COMPILADOR%") do set "GCC_DIR=%%~dpG"
if "%COMPILADOR%"=="gcc" for /f "delims=" %%G in ('where gcc') do (
    set "GCC_DIR=%%~dpG"
    goto :gcc_encontrado
)
:gcc_encontrado
set "PTHREAD_LIB_DIR=%TEMP%\openmp_pthread_%RANDOM%"
if exist "%GCC_DIR%pthreadGC-3.dll" if exist "%GCC_DIR%dlltool.exe" (
    mkdir "%PTHREAD_LIB_DIR%" >nul 2>nul
    "%GCC_DIR%dlltool.exe" --export-all-symbols --dllname pthreadGC-3.dll --output-def "%PTHREAD_LIB_DIR%\pthread.def"
    "%GCC_DIR%dlltool.exe" --dllname pthreadGC-3.dll --def "%PTHREAD_LIB_DIR%\pthread.def" --output-lib "%PTHREAD_LIB_DIR%\libpthread.a"
    if exist "%PTHREAD_LIB_DIR%\libpthread.a" (
        set "LIBPTHREAD=-L%PTHREAD_LIB_DIR%"
        set "PATH=%GCC_DIR%;%PATH%"
    )
)

set "EJECUTABLE=matrices_openmp_opcion1.exe"
set "ARCHIVO_CSV=%CD%\resultados_openmp_opcion1.csv"
set "ARCHIVO_SALIDA=%TEMP%\matrices_openmp_opcion1_%RANDOM%.txt"
set "PREFIJO_TEMP=%TEMP%\matrices_openmp_opcion1_%RANDOM%"

echo Compilando opcion 1: parallel for...
"%COMPILADOR%" %LIBPTHREAD% -std=c11 -O2 -Wall -Wextra -Wpedantic -fopenmp -DOPENMP_OPCION=1 matrices.c -o "%EJECUTABLE%"
if errorlevel 1 (
    echo.
    echo No se pudo enlazar OpenMP. Verifica que GCC y pthreadGC-3.dll esten instalados.
    pause
    exit /b 1
)

for %%O in (500 1000 2000 4000) do (
    type nul > "%PREFIJO_TEMP%_%%O.csv"
)
echo.
echo Iniciando 10 rondas intercaladas sin semilla explicita.
echo Cada ronda ejecuta los ordenes 500, 1000, 2000 y 4000.
echo.

for /l %%R in (1,1,10) do (
    echo ==================================================
    echo Ronda %%R de 10...
    for %%O in (500 1000 2000 4000) do (
        echo Ejecutando orden %%O...
        "%EJECUTABLE%" %%O 500 > "%ARCHIVO_SALIDA%" 2>&1
        if errorlevel 1 (
            echo Error al ejecutar la prueba con orden %%O en la ronda %%R.
            type "%ARCHIVO_SALIDA%"
            del /q "%ARCHIVO_SALIDA%" "%PREFIJO_TEMP%_*.csv" >nul 2>nul
            pause
            exit /b 1
        )
        type "%ARCHIVO_SALIDA%"
        powershell -NoProfile -ExecutionPolicy Bypass -Command "$texto = Get-Content -Raw -LiteralPath '%ARCHIVO_SALIDA%'; $orden = [regex]::Match($texto, 'Orden: (\d+) x \d+').Groups[1].Value; $limite = [regex]::Match($texto, 'Valores aleatorios: 1\.\.(\d+)').Groups[1].Value; $semilla = [regex]::Match($texto, 'Semilla: (\d+)').Groups[1].Value; $tiempo = [regex]::Match($texto, 'Tiempo de multiplicacion: ([0-9.]+) segundos').Groups[1].Value; if ([string]::IsNullOrWhiteSpace($orden) -or [string]::IsNullOrWhiteSpace($tiempo)) { exit 1 }; Add-Content -LiteralPath '%PREFIJO_TEMP%_%%O.csv' -Value ($orden + ',' + $limite + ',' + $semilla + ',' + $tiempo)"
        if errorlevel 1 (
            echo Error al extraer los resultados de la prueba con orden %%O.
            del /q "%ARCHIVO_SALIDA%" "%PREFIJO_TEMP%_*.csv" >nul 2>nul
            pause
            exit /b 1
        )
    )
    echo.
)

> "%ARCHIVO_CSV%" echo orden,limite,semilla,tiempo_segundos
for %%O in (500 1000 2000 4000) do type "%PREFIJO_TEMP%_%%O.csv" >> "%ARCHIVO_CSV%"
del /q "%ARCHIVO_SALIDA%" >nul 2>nul
del /q "%PREFIJO_TEMP%_*.csv" >nul 2>nul
if not exist "%ARCHIVO_CSV%" (
    echo Error: no se pudo generar el archivo CSV.
    pause
    exit /b 1
)
echo ==================================================
echo Pruebas terminadas.
echo Resultados guardados en: %ARCHIVO_CSV%
echo.
type "%ARCHIVO_CSV%"
echo.
pause
