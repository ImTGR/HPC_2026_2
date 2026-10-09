$ErrorActionPreference = 'Stop'

$directorioProyecto = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $directorioProyecto

$compilador = 'gcc'
$gccDisponible = Get-Command gcc -ErrorAction SilentlyContinue
if ($null -eq $gccDisponible -and (Test-Path 'C:\msys64\mingw64\bin\gcc.exe')) {
    $compilador = 'C:\msys64\mingw64\bin\gcc.exe'
    $env:Path = "C:\msys64\mingw64\bin;$env:Path"
}
elseif ($null -eq $gccDisponible) {
    throw 'No se encontro gcc. Agrega GCC al PATH o instala MSYS2 en C:\msys64.'
}

$ejecutable = Join-Path $directorioProyecto 'matrices_secuencial.exe'
$archivoResultados = Join-Path $directorioProyecto 'resultados_matrices_secuencial.csv'
$ordenes = @(500, 1000, 2000, 4000)
$numeroPruebas = 10
$limite = 500
$resultados = [System.Collections.Generic.List[object]]::new()

Write-Host 'Compilando matrices.c sin optimizacion...'
& $compilador -std=c11 -Wall -Wextra -Wpedantic matrices.c -o $ejecutable
if ($LASTEXITCODE -ne 0) {
    throw 'La compilacion fallo.'
}

$totalPruebas = $ordenes.Count * $numeroPruebas
$pruebaActual = 0
foreach ($orden in $ordenes) {
    for ($numeroPrueba = 1; $numeroPrueba -le $numeroPruebas; $numeroPrueba++) {
        $pruebaActual++
        Write-Host ("[{0}/{1}] Orden {2}, prueba {3}..." -f $pruebaActual, $totalPruebas, $orden, $numeroPrueba)

        # No se pasa semilla: el programa usa time(NULL) automaticamente.
        $salida = & $ejecutable $orden $limite 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw ("La prueba de orden {0}, numero {1} fallo: {2}" -f $orden, $numeroPrueba, ($salida -join ' '))
        }

        $lineaTiempo = $salida | Select-String -Pattern 'Tiempo de multiplicacion:\s*([0-9]+(?:[.,][0-9]+)?)\s*segundos'
        if ($null -eq $lineaTiempo) {
            throw ("No se encontro el tiempo en la salida de la prueba de orden {0}, numero {1}." -f $orden, $numeroPrueba)
        }

        $tiempoTexto = $lineaTiempo.Matches[0].Groups[1].Value.Replace(',', '.')
        $tiempo = [double]::Parse($tiempoTexto, [Globalization.CultureInfo]::InvariantCulture)
        $resultados.Add([PSCustomObject]@{
                Orden = $orden
                Prueba = $numeroPrueba
                Limite = $limite
                TiempoSegundos = $tiempo
            })
    }
}

$resultados | Export-Csv -Path $archivoResultados -NoTypeInformation -Encoding UTF8 -Delimiter ';'
Write-Host ''
Write-Host ("Listo. Se guardaron {0} resultados en: {1}" -f $resultados.Count, $archivoResultados)
