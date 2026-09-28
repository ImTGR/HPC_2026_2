$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$compilador = 'gcc'
$ejecutable = Join-Path $PSScriptRoot 'matrices.exe'
$resultadosCsv = Join-Path $PSScriptRoot 'resultados_procesos.csv'

$ordenes = @(500, 1000, 2000, 4000)
$procesos = @(2, 5, 10)
$repeticiones = 10
$limiteAleatorio = 500

Write-Host 'Compilando matrices.c...'
& $compilador -std=c11 -O2 -Wall -Wextra -Wpedantic matrices.c -o $ejecutable
if ($LASTEXITCODE -ne 0) {
    throw 'No fue posible compilar matrices.c.'
}

$resultados = [System.Collections.Generic.List[object]]::new()
$prueba = 0
$totalPruebas = $ordenes.Count * $procesos.Count * $repeticiones

foreach ($cantidadProcesos in $procesos) {
    foreach ($orden in $ordenes) {
        for ($repeticion = 1; $repeticion -le $repeticiones; $repeticion++) {
            $prueba++
            Write-Host "[$prueba/$totalPruebas] Orden=$orden Procesos=$cantidadProcesos Repeticion=$repeticion"

            # No se pasa semilla; el programa usa la hora del sistema.
            $salida = & $ejecutable $orden $limiteAleatorio $cantidadProcesos procesos 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw "Fallo en orden=$orden, procesos=$cantidadProcesos, repeticion=$repeticion."
            }

            $lineaTiempo = $salida | Select-String 'Tiempo de multiplicacion:' | Select-Object -First 1
            if ($null -eq $lineaTiempo -or $lineaTiempo.Line -notmatch '([0-9]+([.,][0-9]+)?)\s+segundos') {
                throw "No se pudo leer el tiempo en orden=$orden, procesos=$cantidadProcesos, repeticion=$repeticion."
            }

            $tiempoTexto = $Matches[1].Replace(',', '.')
            $resultados.Add([pscustomobject]@{
                Prueba = $prueba
                Repeticion = $repeticion
                Orden = $orden
                Procesos = $cantidadProcesos
                LimiteAleatorio = $limiteAleatorio
                TiempoSegundos = [double]::Parse($tiempoTexto, [Globalization.CultureInfo]::InvariantCulture)
            })

            # Evita repetir la misma semilla basada en time(NULL) en dos ejecuciones consecutivas.
            Start-Sleep -Seconds 1
        }
    }
}

$resultados | Export-Csv -Path $resultadosCsv -NoTypeInformation -Encoding UTF8
Write-Host "Pruebas terminadas: $totalPruebas"
Write-Host "Resultados guardados en: $resultadosCsv"
