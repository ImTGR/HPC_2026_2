# Monte Carlo: Needles y Dartboard con OpenMP

Los archivos originales `Needle.c` y `Dartboard.c` se conservan sin cambios.
La continuación del reto está en `montecarlo.c`: permite ejecutar ambos
algoritmos en serial, con OpenMP, con OpenMP + SIMD y con almacenamiento de
muestras para estudiar el costo de memoria.

El análisis de las fórmulas, la validación de los originales y las conclusiones
basadas en mediciones están en [INFORME.md](INFORME.md).

## Requisitos y reproducción

Se necesitan Python 3 y GCC con OpenMP; en Windows se utilizó MinGW-w64 de
MSYS2. Los scripts usan solamente la biblioteca estándar de Python. `gprof`
es opcional. Ejecutar desde esta carpeta, en PowerShell:

```powershell
python build.py
python verify.py
python profile.py
python benchmark.py --repeats 5
```

Opcionalmente, si está instalado `matplotlib`, `python graficas.py` reproduce
las figuras del informe. No se necesita para compilar, validar o medir.

Ejecutar el profiler y el benchmark por separado. Cerrar otras tareas intensivas
y mantener el mismo modo de energía para mejorar la comparación. Se guardan
los comandos exactos en `build/comandos.txt`, los diagnósticos SIMD en
`build/vector_*.txt` y las mediciones en `resultados/`.

Ejemplos de ejecución directa:

```powershell
.\build\mc_O2.exe dartboard serial 10000000 1 42 static
.\build\mc_O2.exe dartboard omp 10000000 4 42 static
.\build\mc_native.exe needles omp 10000000 4 42 static
.\build\mc_native.exe needles simd 10000000 4 42 guided
.\build\mc_native.exe dartboard buffered 10000000 1 42 static
```

Los argumentos son: algoritmo, modo, número de muestras, hilos, semilla y
planificación. `serial` y `buffered` exigen un hilo. `buffered` es una variante
experimental serial que guarda dos arreglos; permite contrastar memoria y
perfilar fases. Los modos `omp` y `simd` generan y consumen cada muestra al
instante, sin arreglos. Todos producen el mismo conteo con el mismo algoritmo,
N y semilla. Se requiere N positivo y representable en `int64_t`; una semilla
es un entero sin signo de 64 bits. Los resultados se imprimen en CSV.

En Linux se usan los mismos scripts; los ejecutables no llevan `.exe`:

```bash
./build/mc_native dartboard omp 10000000 4 42 static
```

Para una ejecución más breve o para otro equipo:

```powershell
python benchmark.py --sizes 1000000 10000000 --threads 1 2 4 --repeats 3
```

## Variantes de compilación

| Nombre | Opciones relevantes | Propósito |
|---|---|---|
| O2 | `-O2` | Base de comparación con optimización habitual |
| O3 | `-O3` | Efecto de elevar el nivel de optimización |
| native | `-O3 -march=native` | Efecto adicional de especializar para esta CPU |
| profile | `-O2 -g -pg -fno-inline` | Intento de profiling con gprof; no usar para speedup |

Todas las variantes nuevas usan `-fopenmp -ffp-contract=off -fno-math-errno`.
La primera habilita OpenMP; la segunda evita diferencias por contracción FMA;
la última permite optimizar funciones matemáticas sin mantener `errno`.
Los argumentos de `sin` están en un intervalo válido y no se consulta `errno`.
No se activa `-ffast-math`. Un ejecutable `native` debe recompilarse si se lleva
a otra CPU. Los originales se compilan aparte con `-O2` y sin OpenMP.

## Archivos y métricas

| Archivo | Contenido |
|---|---|
| `montecarlo.c` | Implementaciones y medición de tiempo/CPU/memoria |
| `verify.py` | Referencia independiente, igualdad entre modos, entradas inválidas y chequeos estadísticos |
| `benchmark.py` | Orden aleatorio, cinco repeticiones, CSV crudo y resumen |
| `profile.py` | Intento gprof y profiling instrumentado de generación/evaluación |
| `graficas.py` | Figuras a partir de los CSV, con matplotlib opcional |
| `resultados/verificacion.json` | Evidencia de verificación y salidas de los originales |
| `resultados/mediciones.csv` | Una fila por ejecución del benchmark |
| `resultados/resumen.csv` | Mediana, dispersión, speedup y eficiencia por configuración |
| `resultados/entorno.json` | CPU, sistema, compilador, parámetros y variables OpenMP |
| `resultados/perfil_fases.csv` | Cinco repeticiones del perfil instrumentado |
| `resultados/perfil_estado.json` | Estado real de gprof y medianas de las fases |

`wall_s` mide el cálculo después de inicializar el equipo OpenMP. Incluye
generación, conteo y reducción; en `buffered` también reserva y liberación.
No incluye el arranque del proceso ni imprimir resultados. En Windows usa
`QueryPerformanceCounter`; en otros sistemas, `omp_get_wtime`.
`process_wall_s` incluye todo el proceso, medido desde Python, y permite ver
el efecto del arranque. `cpu_s` suma tiempo de CPU del proceso; su resolución
puede resultar insuficiente para casos pequeños. `peak_rss_kib` es el pico de
memoria residente del proceso completo, incluyendo runtime, pilas y bibliotecas.
`buffer_bytes` contabiliza exclusivamente los arreglos del algoritmo.

El speedup se calcula contra `serial` del **mismo binario y N**. Para comparar
compiladores, se contrasta el mismo modo y número de hilos. Comparar directamente
un original con `rand()` con una versión nueva mezcla el efecto del RNG con el
del paralelismo. Las repeticiones del benchmark reutilizan la semilla 42 para
medir variación temporal; no son nuevas muestras estadísticas de π.
