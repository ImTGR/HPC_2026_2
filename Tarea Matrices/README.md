# Multiplicación de matrices cuadradas en C

Este programa crea dos matrices cuadradas de enteros positivos, las llena con
valores pseudoaleatorios y calcula `C = A x B` mediante el algoritmo clásico de
tres ciclos anidados. Puede ejecutarse directamente con valores predeterminados
o recibir el tamaño y el límite mediante argumentos. Nunca solicita información
durante la ejecución.

## Forma más sencilla en Windows

Haz doble clic en `ejecutar.bat`. El archivo realiza automáticamente estas tres
acciones:

1. Entra en la carpeta del proyecto.
2. Compila `matrices.c` y genera `matrices.exe` usando GCC.
3. Ejecuta `matrices.exe` y mantiene la ventana abierta para mostrar el resultado.

Esto requiere que el comando `gcc` esté incluido en el `PATH`, pero no requiere
escribir ningún comando manualmente.

## Prueba de órdenes y generación de CSV

Para ejecutar los cuatro casos de medición con valores aleatorios entre `1` y
`500`, sin proporcionar una semilla, haz doble clic en
`prueba_indices.bat`. El script compila el programa y ejecuta los órdenes
`500`, `1000`, `2000` y `4000`. Al terminar, genera
`resultados_indices.csv` con las columnas `orden`, `limite`, `semilla` y
`tiempo_segundos`.

La semilla se omite intencionalmente en cada llamada, por lo que el programa
usa la hora actual. La columna `semilla` se conserva en el CSV para registrar
el valor utilizado en cada ejecución.

## Ejecución directa en Visual Studio

La carpeta incluye un archivo `CMakeLists.txt`, que Visual Studio puede abrir
como proyecto CMake:

1. Abre Visual Studio.
2. Selecciona **Archivo > Abrir > Carpeta**.
3. Selecciona la carpeta `Tarea Matrices`.
4. Espera a que Visual Studio termine de configurar CMake.
5. Selecciona `matrices.exe` como elemento de inicio si Visual Studio lo pide.
6. Presiona **Ctrl+F5** para ejecutar sin depuración o **F5** para depurar.

No hace falta configurar argumentos. Al ejecutarlo así, usa automáticamente una
matriz `3 x 3` con valores entre `1` y `10`.

## Requisitos

- Un compilador compatible con C11, como GCC o Clang.
- `make` es opcional; también se incluye el comando de compilación manual.

## Compilación

Con `make`:

```sh
make
```

Para compilar y ejecutar en una sola orden, se puede pasar la variable `ARGS`:

```sh
make run ARGS="3 10 12345"
```

La semilla es opcional:

```sh
make run ARGS="3 10"
```

También puede ejecutarse sin argumentos, usando los valores predeterminados:

```sh
make run
```

Manualmente con GCC:

```sh
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic matrices.c -o matrices
```

En Windows, el archivo generado normalmente será `matrices.exe`.

## Ejecución sin parámetros

Después de compilar, el ejecutable se puede iniciar directamente:

```powershell
.\matrices.exe
```

En este caso se usan `TAMANO=3` y `LIMITE=10`. Estos valores se encuentran al
principio de `matrices.c` y pueden modificarse si se desean otros valores
predeterminados:

```c
#define ORDEN_PREDETERMINADO 3
#define LIMITE_PREDETERMINADO 10
```

## Ejecución paramétrica opcional

```text
./matrices [TAMANO [LIMITE [SEMILLA]]]
```

- `TAMANO`: número de filas y columnas de las tres matrices.
- `LIMITE`: valor máximo permitido en las matrices de entrada. Cada celda toma
  un valor en el intervalo `1..LIMITE`. Si se omite, se usa `10`.
- `SEMILLA`: entero positivo opcional. Si se proporciona, permite reproducir
  exactamente las mismas matrices; si se omite, se usa la hora actual.

Ejemplos:

```sh
./matrices 3 10
./matrices 3 10 12345
./matrices 3
```

En PowerShell sobre Windows:

```powershell
.\matrices.exe 3 10 12345
```

Las matrices se imprimen cuando `TAMANO` es menor o igual que 10. Para tamaños
mayores se omiten de la salida, porque imprimirlas puede tardar mucho y generar
una cantidad enorme de texto.

## Explicación paso a paso

### 1. Valores predeterminados o lectura de parámetros

Sin argumentos, `main` usa las constantes `ORDEN_PREDETERMINADO` y
`LIMITE_PREDETERMINADO`. Si se proporciona solo `TAMANO`, se usa ese valor y
`LIMITE_PREDETERMINADO`. La función `leer_entero_positivo` usa `strtol` para comprobar que cada argumento sea
realmente un entero decimal, positivo y representable por el tipo `int`.
Entradas como `0`, `-3`, `abc` o un número demasiado grande se rechazan.

### 2. Prevención del desbordamiento aritmético

Cada elemento del resultado se calcula así:

```text
C[i][j] = A[i][0] * B[0][j] + ... + A[i][n-1] * B[n-1][j]
```

Si cada entrada vale como máximo `LIMITE`, el mayor valor posible de una celda
del resultado es:

```text
TAMANO * LIMITE * LIMITE
```

Antes de reservar memoria, el programa verifica el límite usando una comparación
equivalente mediante división. Esto evita que incluso la expresión usada para
comprobar parámetros extremos pueda desbordarse. Si el resultado máximo supera
`INT_MAX`, la ejecución se rechaza. Así, tanto cada producto como su sumatoria
caben en un `int`. Por ejemplo, `./matrices 1000 100` es válido porque el máximo
es 10 000 000, pero `./matrices 1000 2000` se rechaza.

### 3. Reserva dinámica de memoria

El tamaño de las matrices solo se conoce al ejecutar el programa. Por eso no se
declaran como arreglos de tamaño fijo. Cada matriz se representa mediante un
puntero:

```c
int *a;
```

Para una matriz de orden `n` hacen falta `n * n` enteros. La función
`reservar_matriz` reserva un único bloque contiguo:

```c
return malloc(n * n * sizeof(int));
```

Antes de la multiplicación, la función comprueba por separado que `n * n` y la
cantidad de bytes no desborden el tipo `size_t`. `malloc` devuelve la dirección
del bloque o `NULL` si el sistema no puede entregarlo. Se reservan tres bloques,
uno para `A`, otro para `B` y otro para `C`:

```c
a = reservar_matriz(orden);
b = reservar_matriz(orden);
c = reservar_matriz(orden);
```

Esta representación contigua tiene dos ventajas: requiere una sola reserva por
matriz y suele aprovechar mejor la memoria caché que reservar cada fila por
separado.

Aunque conceptualmente se escribe `A[i][j]`, en el bloque lineal la posición se
calcula mediante:

```c
a[i * orden + j]
```

`i * orden` salta hasta el comienzo de la fila `i`, y `+ j` selecciona su
columna. Al terminar, cada bloque se devuelve al sistema exactamente una vez:

```c
free(a);
free(b);
free(c);
```

También se llama a `free` si alguna reserva falla. `free(NULL)` está definido
por C y no causa ningún problema.

### 4. Llenado pseudoaleatorio

`llenar_matriz` recorre las `n * n` posiciones. Se usa un generador pequeño
`xorshift32` y la expresión siguiente garantiza enteros positivos dentro del
límite:

```c
1 + siguiente_aleatorio() % limite
```

Este generador es adecuado para datos de prueba, pero no para criptografía.

### 5. Multiplicación

El algoritmo clásico usa tres ciclos:

- `i` recorre las filas de `A`.
- `j` recorre las columnas de `B`.
- `k` acumula los productos que forman `C[i][j]`.

Su complejidad temporal es `O(n^3)` y las tres matrices ocupan en total
`3 * n^2 * sizeof(int)` bytes, por lo que la complejidad espacial es `O(n^2)`.

### 6. Medición y liberación

`clock()` mide el tiempo empleado en la multiplicación. Después de mostrar el
resultado o el resumen, el programa libera toda la memoria dinámica antes de
terminar.

## Casos de prueba sugeridos

```sh
# Caso pequeño y reproducible
./matrices 2 5 1

# Parámetro inválido
./matrices 0 5

# Riesgo de desbordamiento de int
./matrices 1000 2000

# Matriz suficientemente grande para omitir su impresión
./matrices 500 20 42
```

## OpenMP: comandos y lugar de uso

Las directivas se deben colocar en la función `multiplicar_matrices`, justo
antes de los ciclos que empiezan en `for (i = 0; i < orden; ++i)`.

Para repartir las filas entre los hilos, usar:

```c
#pragma omp parallel for schedule(static)
```

Para repartir juntos los ciclos `i` y `j`, usar como alternativa:

```c
#pragma omp parallel for collapse(2) schedule(static)
```

Para vectorizar el ciclo interno `k`, colocar esta directiva justo antes de
`for (k = 0; k < orden; ++k)`:

```c
#pragma omp simd reduction(+:suma)
```

La primera opción recomendada es `parallel for`, porque cada hilo calcula
filas diferentes de la matriz `c`. `collapse(2)` puede servir cuando hay pocas
filas para repartir. `simd` intenta acelerar las operaciones del ciclo `k`;
`reduction` permite sumar correctamente los productos.

Después de añadir las directivas al código, compilar `matrices.c` con GCC así:

```sh
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -fopenmp matrices.c -o matrices_openmp
```

La opción `-fopenmp` activa OpenMP. En Windows PowerShell se usa:

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -fopenmp matrices.c -o matrices_openmp.exe
$env:OMP_NUM_THREADS = "4"
.\matrices_openmp.exe 1000 20 42
```

En Linux o macOS, el número de hilos se puede indicar así:

```sh
OMP_NUM_THREADS=4 ./matrices_openmp 1000 20 42
```

Para comparar las tres opciones con los órdenes `500`, `1000`, `2000` y `4000`,
y valores aleatorios entre `1` y `500` sin proporcionar una semilla, usa estos
scripts en Windows. Cada script realiza 10 rondas intercaladas: en cada ronda
ejecuta una vez cada orden, evitando ejecutar las 10 mediciones del mismo orden
de forma consecutiva. Al finalizar, el CSV reorganiza los resultados para
guardar las 10 mediciones de cada orden juntas.

- `openmp_opcion1_indices.bat`: `parallel for schedule(static)`.
- `openmp_opcion2_indices.bat`: `parallel for collapse(2) schedule(static)`.
- `openmp_opcion3_indices.bat`: `simd reduction(+:suma)`.

Cada script compila su propia variante con `-DOPENMP_OPCION=1`, `2` o `3`,
respectivamente, y `-fopenmp`. También ejecuta los cuatro órdenes y guarda los
resultados en un CSV distinto (`resultados_openmp_opcion1.csv`,
`resultados_openmp_opcion2.csv` o `resultados_openmp_opcion3.csv`), por lo que
se pueden ejecutar sin sobrescribirse entre sí. La semilla se omite
intencionalmente en cada llamada; el programa usa la hora actual y registra la
semilla efectiva en el CSV.

La opción 1 es la recomendada para repartir filas entre hilos. `collapse(2)`
puede ser útil cuando hay pocas filas para repartir. `simd` vectoriza el ciclo
interno y no crea un equipo de hilos por sí sola. En matrices pequeñas, crear
los hilos puede costar más que la propia multiplicación.

En Windows se recomienda MSYS2 MinGW-w64 para que GCC incluya las bibliotecas
necesarias para OpenMP. Los scripts también son compatibles con el MinGW.org
instalado en `C:\MinGW`: si encuentra `pthreadGC-3.dll` y `dlltool.exe`,
generan automáticamente en una carpeta temporal el import library que falta
(`libpthread.a`) y lo usan durante el enlace.

Si se instala MSYS2, desde la terminal **MSYS2 UCRT64** se puede instalar el
compilador con:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Los scripts priorizan `C:\msys64\ucrt64\bin\gcc.exe` y
`C:\msys64\mingw64\bin\gcc.exe` si existen. Con MinGW.org, la biblioteca DLL
`pthreadGC-3.dll` debe estar junto a `gcc.exe`.
