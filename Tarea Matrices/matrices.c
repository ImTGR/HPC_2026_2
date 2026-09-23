#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ORDEN_PREDETERMINADO 3
#define LIMITE_PREDETERMINADO 10
#define HILOS_PREDETERMINADOS 4

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
    size_t i;
    size_t j;
    size_t k;

    for (i = tarea->fila_inicio; i < tarea->fila_inicio + tarea->filas; ++i)
    {
        for (j = 0; j < tarea->orden; ++j)
        {
            int suma = 0;

            for (k = 0; k < tarea->orden; ++k)
            {
                suma += tarea->a[i * tarea->orden + k] *
                        tarea->b[k * tarea->orden + j];
            }
            tarea->c[i * tarea->orden + j] = suma;
        }
    }

    return 0;
}
#else
static void *multiplicar_filas_thread(void *arg)
{
    TareaMultiplicacion *tarea = (TareaMultiplicacion *)arg;
    size_t i;
    size_t j;
    size_t k;

    for (i = tarea->fila_inicio; i < tarea->fila_inicio + tarea->filas; ++i)
    {
        for (j = 0; j < tarea->orden; ++j)
        {
            int suma = 0;

            for (k = 0; k < tarea->orden; ++k)
            {
                suma += tarea->a[i * tarea->orden + k] *
                        tarea->b[k * tarea->orden + j];
            }
            tarea->c[i * tarea->orden + j] = suma;
        }
    }

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

int main(int argc, char *argv[])
{
    int orden_int;
    int limite;
    int hilos_int;
    int semilla_int = 0;
    size_t orden;
    uint32_t semilla;
    unsigned int hilos;
    int *a = NULL;
    int *b = NULL;
    int *c = NULL;
    clock_t inicio;
    clock_t fin;

    if (argc < 1 || argc > 5)
    {
        fprintf(stderr, "Uso: %s [TAMANO [LIMITE [HILOS [SEMILLA]]]]\n",
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
        if (argc >= 5 && !leer_entero_positivo(argv[4], &semilla_int))
        {
            fprintf(stderr, "Error: SEMILLA debe ser un entero positivo.\n");
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

    if (argc >= 5)
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

    inicio = clock();
    if (hilos > 1)
    {
        multiplicar_matrices_paralelo(a, b, c, orden, hilos);
    }
    else
    {
        multiplicar_matrices(a, b, c, orden);
    }
    fin = clock();

    printf("Orden: %d x %d\n", orden_int, orden_int);
    printf("Valores aleatorios: 1..%d\n", limite);
    printf("Semilla: %u\n", semilla);
    printf("Hilos: %u\n", hilos);
    printf("Tiempo de multiplicacion: %.12f segundos\n\n",
           (double)(fin - inicio) / CLOCKS_PER_SEC);

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
