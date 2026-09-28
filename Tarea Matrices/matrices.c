#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ORDEN_PREDETERMINADO 3
#define LIMITE_PREDETERMINADO 10
#define HILOS_PREDETERMINADOS 4

enum ModoEjecucion
{
    MODO_SECUENCIAL,
    MODO_HILOS,
    MODO_PROCESOS
};

/* Estado del generador pseudoaleatorio xorshift32. */
static uint32_t rng_state;

static uint32_t siguiente_aleatorio(void)
{
    uint32_t x = rng_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static double tiempo_actual(void)
{
#ifdef _WIN32
    LARGE_INTEGER frecuencia;
    LARGE_INTEGER contador;

    QueryPerformanceFrequency(&frecuencia);
    QueryPerformanceCounter(&contador);
    return (double)contador.QuadPart / (double)frecuencia.QuadPart;
#else
    struct timespec tiempo;

    clock_gettime(CLOCK_MONOTONIC, &tiempo);
    return (double)tiempo.tv_sec + (double)tiempo.tv_nsec / 1000000000.0;
#endif
}

static int leer_entero_positivo(const char *texto, int *valor)
{
    char *fin;
    long numero;

    errno = 0;
    numero = strtol(texto, &fin, 10);
    if (errno != 0 || *texto == '\0' || *fin != '\0' ||
        numero <= 0 || numero > INT_MAX)
    {
        return 0;
    }

    *valor = (int)numero;
    return 1;
}

static int *reservar_matriz(size_t orden)
{
    size_t cantidad;

    /* Evita que orden * orden o cantidad * sizeof(int) se desborden. */
    if (orden > SIZE_MAX / orden)
    {
        return NULL;
    }
    cantidad = orden * orden;
    if (cantidad > SIZE_MAX / sizeof(int))
    {
        return NULL;
    }

    return malloc(cantidad * sizeof(int));
}

static void llenar_matriz(int *matriz, size_t orden, int limite)
{
    size_t total = orden * orden;
    size_t i;

    for (i = 0; i < total; ++i)
    {
        matriz[i] = 1 + (int)(siguiente_aleatorio() % (uint32_t)limite);
    }
}

static void multiplicar_matrices(const int *a, const int *b, int *c,
                                 size_t orden)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < orden; ++i)
    {
        for (j = 0; j < orden; ++j)
        {
            int suma = 0;

            for (k = 0; k < orden; ++k)
            {
                suma += a[i * orden + k] * b[k * orden + j];
            }
            c[i * orden + j] = suma;
        }
    }
}

static void multiplicar_rango(const int *a, const int *b, int *c,
                              size_t orden, size_t fila_inicio, size_t filas)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = fila_inicio; i < fila_inicio + filas; ++i)
    {
        for (j = 0; j < orden; ++j)
        {
            int suma = 0;

            for (k = 0; k < orden; ++k)
            {
                suma += a[i * orden + k] * b[k * orden + j];
            }
            c[i * orden + j] = suma;
        }
    }
}

typedef struct
{
    const int *a;
    const int *b;
    int *c;
    size_t orden;
    size_t fila_inicio;
    size_t filas;
} TareaMultiplicacion;

#ifdef _WIN32
static DWORD WINAPI multiplicar_filas_thread(LPVOID arg)
{
    TareaMultiplicacion *tarea = (TareaMultiplicacion *)arg;

    multiplicar_rango(tarea->a, tarea->b, tarea->c, tarea->orden,
                      tarea->fila_inicio, tarea->filas);

    return 0;
}
#else
static void *multiplicar_filas_thread(void *arg)
{
    TareaMultiplicacion *tarea = (TareaMultiplicacion *)arg;

    multiplicar_rango(tarea->a, tarea->b, tarea->c, tarea->orden,
                      tarea->fila_inicio, tarea->filas);

    return NULL;
}
#endif

static void multiplicar_matrices_paralelo(const int *a, const int *b, int *c,
                                          size_t orden, unsigned int hilos)
{
    unsigned int total_hilos;
    size_t filas_por_hilo;
    size_t filas_restantes;
    size_t fila_actual;
    unsigned int i;

    if (hilos == 0)
    {
        hilos = 1;
    }

    total_hilos = (unsigned int)orden < hilos ? (unsigned int)orden : hilos;
    if (total_hilos == 0)
    {
        total_hilos = 1;
    }

    filas_por_hilo = orden / total_hilos;
    filas_restantes = orden % total_hilos;
    fila_actual = 0;

#ifdef _WIN32
    HANDLE *threads = calloc((size_t)total_hilos, sizeof(*threads));
    TareaMultiplicacion *tareas = calloc((size_t)total_hilos, sizeof(*tareas));
    if (threads == NULL || tareas == NULL)
    {
        fprintf(stderr,
                "Error: no fue posible reservar memoria para los hilos de trabajo.\n");
        free(threads);
        free(tareas);
        multiplicar_matrices(a, b, c, orden);
        return;
    }

    for (i = 0; i < total_hilos; ++i)
    {
        size_t filas_de_este_hilo = filas_por_hilo + (i < filas_restantes ? 1U : 0U);

        tareas[i].a = a;
        tareas[i].b = b;
        tareas[i].c = c;
        tareas[i].orden = orden;
        tareas[i].fila_inicio = fila_actual;
        tareas[i].filas = filas_de_este_hilo;

        threads[i] = CreateThread(NULL, 0, multiplicar_filas_thread,
                                  &tareas[i], 0, NULL);
        if (threads[i] == NULL)
        {
            fprintf(stderr, "Error: no fue posible crear el hilo %u.\n", i);
            for (unsigned int j = 0; j < i; ++j)
            {
                WaitForSingleObject(threads[j], INFINITE);
            }
            free(threads);
            free(tareas);
            multiplicar_matrices(a, b, c, orden);
            return;
        }

        fila_actual += filas_de_este_hilo;
    }

    for (i = 0; i < total_hilos; ++i)
    {
        WaitForSingleObject(threads[i], INFINITE);
    }

    free(threads);
    free(tareas);
#else
    pthread_t *threads = calloc((size_t)total_hilos, sizeof(*threads));
    TareaMultiplicacion *tareas = calloc((size_t)total_hilos, sizeof(*tareas));
    if (threads == NULL || tareas == NULL)
    {
        fprintf(stderr,
                "Error: no fue posible reservar memoria para los hilos de trabajo.\n");
        free(threads);
        free(tareas);
        multiplicar_matrices(a, b, c, orden);
        return;
    }

    for (i = 0; i < total_hilos; ++i)
    {
        size_t filas_de_este_hilo = filas_por_hilo + (i < filas_restantes ? 1U : 0U);

        tareas[i].a = a;
        tareas[i].b = b;
        tareas[i].c = c;
        tareas[i].orden = orden;
        tareas[i].fila_inicio = fila_actual;
        tareas[i].filas = filas_de_este_hilo;

        if (pthread_create(&threads[i], NULL, multiplicar_filas_thread,
                           &tareas[i]) != 0)
        {
            fprintf(stderr, "Error: no fue posible crear el hilo %u.\n", i);
            free(threads);
            free(tareas);
            multiplicar_matrices(a, b, c, orden);
            return;
        }

        fila_actual += filas_de_este_hilo;
    }

    for (i = 0; i < total_hilos; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(tareas);
#endif
}

static void multiplicar_matrices_procesos(const int *a, const int *b, int *c,
                                          size_t orden, unsigned int procesos)
{
    size_t cantidad = orden * orden;
    size_t bytes = 3 * cantidad * sizeof(int);
    unsigned int total_procesos;
    size_t filas_por_proceso;
    size_t filas_restantes;
    size_t fila_actual = 0;
    unsigned int i;

    if (procesos == 0)
    {
        procesos = 1;
    }
    total_procesos = (unsigned int)orden < procesos ? (unsigned int)orden : procesos;
    if (total_procesos == 0)
    {
        total_procesos = 1;
    }
    filas_por_proceso = orden / total_procesos;
    filas_restantes = orden % total_procesos;

#ifdef _WIN32
    char nombre_mapeo[128];
    char ruta_ejecutable[MAX_PATH];
    HANDLE mapeo;
    int *compartido;
    PROCESS_INFORMATION *informacion;
    DWORD bytes_altos = (DWORD)(((uint64_t)bytes) >> 32);
    DWORD bytes_bajos = (DWORD)((uint64_t)bytes & UINT32_MAX);

    snprintf(nombre_mapeo, sizeof(nombre_mapeo), "Local\\Matrices_%lu",
             (unsigned long)GetCurrentProcessId());
    mapeo = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                               bytes_altos, bytes_bajos, nombre_mapeo);
    if (mapeo == NULL)
    {
        fprintf(stderr, "Error: no fue posible crear memoria compartida.\n");
        multiplicar_matrices(a, b, c, orden);
        return;
    }
    compartido = MapViewOfFile(mapeo, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (compartido == NULL || GetModuleFileNameA(NULL, ruta_ejecutable, MAX_PATH) == 0)
    {
        fprintf(stderr, "Error: no fue posible preparar memoria compartida.\n");
        if (compartido != NULL)
        {
            UnmapViewOfFile(compartido);
        }
        CloseHandle(mapeo);
        multiplicar_matrices(a, b, c, orden);
        return;
    }
    memcpy(compartido, a, cantidad * sizeof(int));
    memcpy(compartido + cantidad, b, cantidad * sizeof(int));

    informacion = calloc((size_t)total_procesos, sizeof(*informacion));
    if (informacion == NULL)
    {
        fprintf(stderr, "Error: no fue posible reservar procesos.\n");
        UnmapViewOfFile(compartido);
        CloseHandle(mapeo);
        multiplicar_matrices(a, b, c, orden);
        return;
    }
    for (i = 0; i < total_procesos; ++i)
    {
        size_t filas = filas_por_proceso + (i < filas_restantes ? 1U : 0U);
        char comando[512];
        STARTUPINFOA inicio;

        ZeroMemory(&inicio, sizeof(inicio));
        inicio.cb = sizeof(inicio);
        snprintf(comando, sizeof(comando),
                 "\"%s\" --proceso-hijo \"%s\" %zu %zu %zu",
                 ruta_ejecutable, nombre_mapeo, orden, fila_actual, filas);
        if (!CreateProcessA(NULL, comando, NULL, NULL, FALSE, 0, NULL, NULL,
                            &inicio, &informacion[i]))
        {
            fprintf(stderr, "Error: no fue posible crear el proceso %u.\n", i);
            for (unsigned int j = 0; j < i; ++j)
            {
                WaitForSingleObject(informacion[j].hProcess, INFINITE);
                CloseHandle(informacion[j].hThread);
                CloseHandle(informacion[j].hProcess);
            }
            free(informacion);
            UnmapViewOfFile(compartido);
            CloseHandle(mapeo);
            multiplicar_matrices(a, b, c, orden);
            return;
        }
        fila_actual += filas;
    }
    for (i = 0; i < total_procesos; ++i)
    {
        WaitForSingleObject(informacion[i].hProcess, INFINITE);
        CloseHandle(informacion[i].hThread);
        CloseHandle(informacion[i].hProcess);
    }
    memcpy(c, compartido + 2 * cantidad, cantidad * sizeof(int));
    free(informacion);
    UnmapViewOfFile(compartido);
    CloseHandle(mapeo);
#else
    int *compartido = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    pid_t *hijos = calloc((size_t)total_procesos, sizeof(*hijos));

    if (compartido == MAP_FAILED || hijos == NULL)
    {
        fprintf(stderr, "Error: no fue posible preparar los procesos.\n");
        if (compartido != MAP_FAILED)
        {
            munmap(compartido, bytes);
        }
        free(hijos);
        multiplicar_matrices(a, b, c, orden);
        return;
    }
    memcpy(compartido, a, cantidad * sizeof(int));
    memcpy(compartido + cantidad, b, cantidad * sizeof(int));
    for (i = 0; i < total_procesos; ++i)
    {
        size_t filas = filas_por_proceso + (i < filas_restantes ? 1U : 0U);
        hijos[i] = fork();
        if (hijos[i] < 0)
        {
            fprintf(stderr, "Error: no fue posible crear el proceso %u.\n", i);
            for (unsigned int j = 0; j < i; ++j)
            {
                waitpid(hijos[j], NULL, 0);
            }
            free(hijos);
            munmap(compartido, bytes);
            multiplicar_matrices(a, b, c, orden);
            return;
        }
        if (hijos[i] == 0)
        {
            multiplicar_rango(compartido, compartido + cantidad,
                              compartido + 2 * cantidad, orden,
                              fila_actual, filas);
            munmap(compartido, bytes);
            _exit(EXIT_SUCCESS);
        }
        fila_actual += filas;
    }
    for (i = 0; i < total_procesos; ++i)
    {
        waitpid(hijos[i], NULL, 0);
    }
    memcpy(c, compartido + 2 * cantidad, cantidad * sizeof(int));
    free(hijos);
    munmap(compartido, bytes);
#endif
}

#ifdef _WIN32
static int ejecutar_proceso_hijo(int argc, char *argv[])
{
    HANDLE mapeo;
    int *compartido;
    size_t orden;
    size_t fila_inicio;
    size_t filas;
    size_t cantidad;
    size_t bytes;

    if (argc != 6 || strcmp(argv[1], "--proceso-hijo") != 0)
    {
        return 0;
    }
    orden = (size_t)strtoull(argv[3], NULL, 10);
    fila_inicio = (size_t)strtoull(argv[4], NULL, 10);
    filas = (size_t)strtoull(argv[5], NULL, 10);
    cantidad = orden * orden;
    bytes = 3 * cantidad * sizeof(int);
    mapeo = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, argv[2]);
    if (mapeo == NULL)
    {
        return EXIT_FAILURE;
    }
    compartido = MapViewOfFile(mapeo, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (compartido == NULL)
    {
        CloseHandle(mapeo);
        return EXIT_FAILURE;
    }
    multiplicar_rango(compartido, compartido + cantidad,
                      compartido + 2 * cantidad, orden, fila_inicio, filas);
    UnmapViewOfFile(compartido);
    CloseHandle(mapeo);
    return EXIT_SUCCESS;
}
#endif

static void imprimir_matriz(const char *nombre, const int *matriz,
                            size_t orden)
{
    size_t i;
    size_t j;

    printf("%s:\n", nombre);
    for (i = 0; i < orden; ++i)
    {
        for (j = 0; j < orden; ++j)
        {
            printf("%8d", matriz[i * orden + j]);
        }
        putchar('\n');
    }
    putchar('\n');
}

static int leer_modo(const char *texto, enum ModoEjecucion *modo)
{
    if (strcmp(texto, "secuencial") == 0)
    {
        *modo = MODO_SECUENCIAL;
        return 1;
    }
    if (strcmp(texto, "hilos") == 0)
    {
        *modo = MODO_HILOS;
        return 1;
    }
    if (strcmp(texto, "procesos") == 0)
    {
        *modo = MODO_PROCESOS;
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int orden_int;
    int limite;
    int hilos_int;
    int semilla_int = 0;
    int semilla_proporcionada = 0;
    enum ModoEjecucion modo = MODO_HILOS;
    size_t orden;
    uint32_t semilla;
    unsigned int hilos;
    int *a = NULL;
    int *b = NULL;
    int *c = NULL;
    double inicio;
    double fin;

#ifdef _WIN32
    if (argc >= 2 && strcmp(argv[1], "--proceso-hijo") == 0)
    {
        return ejecutar_proceso_hijo(argc, argv);
    }
#endif

    if (argc < 1 || argc > 6)
    {
        fprintf(stderr,
                "Uso: %s [TAMANO [LIMITE [HILOS [SEMILLA [MODO]]]]]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 1)
    {
        orden_int = ORDEN_PREDETERMINADO;
        limite = LIMITE_PREDETERMINADO;
        hilos_int = HILOS_PREDETERMINADOS;
        printf("No se recibieron parametros; se usaran TAMANO=%d y LIMITE=%d.\n\n",
               orden_int, limite);
    }
    else
    {
        limite = LIMITE_PREDETERMINADO;
        hilos_int = HILOS_PREDETERMINADOS;
        if (!leer_entero_positivo(argv[1], &orden_int))
        {
            fprintf(stderr, "Error: TAMANO debe ser un entero positivo.\n");
            return EXIT_FAILURE;
        }
        if (argc >= 3 && !leer_entero_positivo(argv[2], &limite))
        {
            fprintf(stderr, "Error: LIMITE debe ser un entero positivo.\n");
            return EXIT_FAILURE;
        }
        if (argc >= 4 && !leer_entero_positivo(argv[3], &hilos_int))
        {
            fprintf(stderr, "Error: HILOS debe ser un entero positivo.\n");
            return EXIT_FAILURE;
        }
        if (argc >= 5 && leer_modo(argv[4], &modo))
        {
            semilla_proporcionada = 0;
        }
        else if (argc >= 5 && !leer_entero_positivo(argv[4], &semilla_int))
        {
            fprintf(stderr, "Error: SEMILLA debe ser un entero positivo.\n");
            return EXIT_FAILURE;
        }
        else if (argc >= 5)
        {
            semilla_proporcionada = 1;
        }
        if (argc >= 6 && !leer_modo(argv[5], &modo))
        {
            fprintf(stderr,
                    "Error: MODO debe ser secuencial, hilos o procesos.\n");
            return EXIT_FAILURE;
        }
    }

    orden = (size_t)orden_int;
    hilos = (unsigned int)hilos_int;

    if (hilos > orden)
    {
        hilos = (unsigned int)orden;
    }

    if (hilos == 0)
    {
        hilos = 1;
    }

    /*
     * El mayor resultado posible es orden * limite^2. Se compara mediante
     * division para que ni siquiera la propia comprobacion se desborde.
     */
    if ((uint64_t)limite * (uint64_t)limite >
        (uint64_t)INT_MAX / (uint64_t)orden)
    {
        fprintf(stderr,
                "Error: TAMANO * LIMITE^2 debe ser menor o igual que %d "
                "para evitar desbordamiento.\n",
                INT_MAX);
        return EXIT_FAILURE;
    }

    if (semilla_proporcionada)
    {
        semilla = (uint32_t)semilla_int;
    }
    else
    {
        semilla = (uint32_t)time(NULL);
    }
    rng_state = semilla;

    a = reservar_matriz(orden);
    b = reservar_matriz(orden);
    c = reservar_matriz(orden);
    if (a == NULL || b == NULL || c == NULL)
    {
        fprintf(stderr, "Error: no fue posible reservar memoria para las matrices.\n");
        free(a);
        free(b);
        free(c);
        return EXIT_FAILURE;
    }

    llenar_matriz(a, orden, limite);
    llenar_matriz(b, orden, limite);

    inicio = tiempo_actual();
    if (modo == MODO_PROCESOS)
    {
        multiplicar_matrices_procesos(a, b, c, orden, hilos);
    }
    else if (modo == MODO_HILOS && hilos > 1)
    {
        multiplicar_matrices_paralelo(a, b, c, orden, hilos);
    }
    else
    {
        multiplicar_matrices(a, b, c, orden);
    }
    fin = tiempo_actual();

    printf("Orden: %d x %d\n", orden_int, orden_int);
    printf("Valores aleatorios: 1..%d\n", limite);
    printf("Semilla: %u\n", semilla);
    printf("Hilos: %u\n", hilos);
    printf("Modo: %s\n",
           modo == MODO_PROCESOS ? "procesos" : modo == MODO_SECUENCIAL ? "secuencial"
                                                                        : "hilos");
    printf("Tiempo de multiplicacion: %.12f segundos\n\n", fin - inicio);

    if (orden <= 10)
    {
        imprimir_matriz("Matriz A", a, orden);
        imprimir_matriz("Matriz B", b, orden);
        imprimir_matriz("Resultado C = A x B", c, orden);
    }
    else
    {
        printf("Las matrices no se muestran porque TAMANO es mayor que 10.\n");
    }

    free(a);
    free(b);
    free(c);
    return EXIT_SUCCESS;
}
