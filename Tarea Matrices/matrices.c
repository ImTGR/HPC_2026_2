#include <mpi.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ORDEN_PREDETERMINADO 3
#define LIMITE_PREDETERMINADO 10

typedef struct
{
    int orden;
    int limite;
    uint32_t semilla;
} Configuracion;

typedef struct
{
    size_t inicio;
    size_t cantidad;
} Filas;

static int leer_entero_positivo(const char *texto, int *valor)
{
    char *fin;
    errno = 0;
    long numero = strtol(texto, &fin, 10);

    if (errno != 0 || *texto == '\0' || *fin != '\0' ||
        numero <= 0 || numero > INT_MAX)
    {
        return 0;
    }

    *valor = (int)numero;
    return 1;
}

static int leer_configuracion(int argc, char *argv[], Configuracion *config)
{
    if (argc > 4)
    {
        fprintf(stderr, "Uso: %s [TAMANO [LIMITE [SEMILLA]]]\n", argv[0]);
        return 0;
    }

    if (argc == 1)
    {
        printf("No se recibieron parametros; se usaran TAMANO=%d y LIMITE=%d.\n\n",
               config->orden, config->limite);
    }
    if (argc >= 2 && !leer_entero_positivo(argv[1], &config->orden))
    {
        fprintf(stderr, "Error: TAMANO debe ser un entero positivo.\n");
        return 0;
    }
    if (argc >= 3 && !leer_entero_positivo(argv[2], &config->limite))
    {
        fprintf(stderr, "Error: LIMITE debe ser un entero positivo.\n");
        return 0;
    }

    size_t orden = (size_t)config->orden;
    if (orden > SIZE_MAX / orden || orden * orden > (size_t)INT_MAX)
    {
        fprintf(stderr, "Error: TAMANO^2 supera el limite de elementos permitido por MPI.\n");
        return 0;
    }
    if ((uint64_t)config->limite * (uint64_t)config->limite >
        (uint64_t)INT_MAX / (uint64_t)orden)
    {
        fprintf(stderr, "Error: TAMANO * LIMITE^2 debe ser menor o igual que %d.\n", INT_MAX);
        return 0;
    }

    if (argc == 4)
    {
        int semilla;
        if (!leer_entero_positivo(argv[3], &semilla))
        {
            fprintf(stderr, "Error: SEMILLA debe ser un entero positivo.\n");
            return 0;
        }
        config->semilla = (uint32_t)semilla;
    }
    else
    {
        config->semilla = (uint32_t)time(NULL);
    }
    return 1;
}

static int configurar_matrices(int argc, char *argv[], int rank, Configuracion *config)
{
    int ok = 1;
    if (rank == 0)
    {
        *config = (Configuracion){ORDEN_PREDETERMINADO, LIMITE_PREDETERMINADO, 0};
        ok = leer_configuracion(argc, argv, config);
    }

    MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (ok)
    {
        MPI_Bcast(&config->orden, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&config->limite, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&config->semilla, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);
    }
    return ok;
}

static Filas repartir_filas(size_t orden, int procesos, int rank)
{
    size_t base = orden / (size_t)procesos;
    size_t sobrantes = orden % (size_t)procesos;
    size_t proceso = (size_t)rank;

    Filas filas = {
        proceso * base + (proceso < sobrantes ? proceso : sobrantes),
        base + (proceso < sobrantes ? 1 : 0)
    };
    return filas;
}

static int *reservar_vector(size_t cantidad)
{
    if (cantidad == 0)
    {
        cantidad = 1;
    }
    if (cantidad > SIZE_MAX / sizeof(int))
    {
        return NULL;
    }
    return malloc(cantidad * sizeof(int));
}

static int todos_ok(int ok_local)
{
    int ok_global;
    MPI_Allreduce(&ok_local, &ok_global, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    return ok_global;
}

static void llenar_matriz(int *matriz, size_t total, int limite, uint32_t *estado)
{
    uint32_t x = *estado;
    for (size_t i = 0; i < total; ++i)
    {
        /* Generador xorshift32. */
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        matriz[i] = 1 + (int)(x % (uint32_t)limite);
    }
    *estado = x;
}

static void generar_matrices(int *a, int *b, size_t total, Configuracion config)
{
    uint32_t estado = config.semilla;
    llenar_matriz(a, total, config.limite, &estado);
    llenar_matriz(b, total, config.limite, &estado);
}

/* Cada proceso calcula exclusivamente las filas que recibio. */
static void multiplicar_filas(const int *a_local, const int *b, int *c_local,
                              size_t filas_local, size_t orden)
{
    for (size_t i = 0; i < filas_local; ++i)
    {
        for (size_t j = 0; j < orden; ++j)
        {
            int suma = 0;
            for (size_t k = 0; k < orden; ++k)
            {
                suma += a_local[i * orden + k] * b[k * orden + j];
            }
            c_local[i * orden + j] = suma;
        }
    }
}

static void imprimir_matriz(const char *nombre, const int *matriz, size_t orden)
{
    printf("%s:\n", nombre);
    for (size_t i = 0; i < orden; ++i)
    {
        for (size_t j = 0; j < orden; ++j)
        {
            printf("%8d", matriz[i * orden + j]);
        }
        putchar('\n');
    }
    putchar('\n');
}

static void presentar_resultados(const char *version, int procesos, Configuracion config,
                                 const int *a, const int *b, const int *c,
                                 double tiempo_calculo, double tiempo_total)
{
    size_t orden = (size_t)config.orden;
    uint64_t huella = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < orden * orden; ++i)
    {
        huella = (huella ^ (uint64_t)c[i]) * UINT64_C(1099511628211);
    }

    printf("\n========== RESULTADOS %s ==========\n\n", version);
    printf("Procesos utilizados: %d\n", procesos);
    printf("Orden: %d x %d\n", config.orden, config.orden);
    printf("Valores aleatorios: 1..%d\n", config.limite);
    printf("Semilla: %" PRIu32 "\n", config.semilla);
    printf("Tiempo de multiplicacion paralela: %.6f segundos\n", tiempo_calculo);
    printf("Tiempo de distribucion, calculo y recoleccion: %.6f segundos\n", tiempo_total);
    printf("Huella de C: %016" PRIx64 "\n\n", huella);

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
    printf("===================================\n");
}

int main(int argc, char *argv[])
{
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
    {
        fprintf(stderr, "Error al inicializar MPI.\n");
        return EXIT_FAILURE;
    }

    int rank, procesos;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procesos);

    Configuracion config;
    if (!configurar_matrices(argc, argv, rank, &config))
    {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    size_t orden = (size_t)config.orden;
    size_t total = orden * orden;
    Filas filas = repartir_filas(orden, procesos, rank);
    size_t elementos_local = filas.cantidad * orden;

    int *a = NULL;
    int *b = reservar_vector(total);
    int *c = NULL;
    int *a_local = reservar_vector(elementos_local);
    int *c_local = reservar_vector(elementos_local);
    int *cantidades = NULL;
    int *desplazamientos = NULL;

    int memoria_ok = b != NULL && a_local != NULL && c_local != NULL;
    if (rank == 0)
    {
        a = reservar_vector(total);
        c = reservar_vector(total);
        cantidades = reservar_vector((size_t)procesos);
        desplazamientos = reservar_vector((size_t)procesos);
        memoria_ok = memoria_ok && a != NULL && c != NULL &&
                     cantidades != NULL && desplazamientos != NULL;
    }

    int salida = EXIT_FAILURE;
    if (!todos_ok(memoria_ok))
    {
        if (rank == 0)
        {
            fprintf(stderr, "Error: no fue posible reservar memoria en todos los procesos.\n");
        }
        goto finalizar;
    }

    if (rank == 0)
    {
        generar_matrices(a, b, total, config);
        for (int p = 0; p < procesos; ++p)
        {
            Filas bloque = repartir_filas(orden, procesos, p);
            cantidades[p] = (int)(bloque.cantidad * orden);
            desplazamientos[p] = (int)(bloque.inicio * orden);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio_total = MPI_Wtime();

    MPI_Bcast(b, (int)total, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv(a, cantidades, desplazamientos, MPI_INT,
                 a_local, (int)elementos_local, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio_calculo = MPI_Wtime();
    multiplicar_filas(a_local, b, c_local, filas.cantidad, orden);
    double calculo_local = MPI_Wtime() - inicio_calculo;

    MPI_Gatherv(c_local, (int)elementos_local, MPI_INT, c, cantidades,
                desplazamientos, MPI_INT, 0, MPI_COMM_WORLD);
    double tiempo_total = MPI_Wtime() - inicio_total;

    double tiempo_calculo;
    MPI_Reduce(&calculo_local, &tiempo_calculo, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        presentar_resultados("MPI", procesos, config, a, b, c,
                             tiempo_calculo, tiempo_total);
    }
    salida = EXIT_SUCCESS;

finalizar:
    free(a);
    free(b);
    free(c);
    free(a_local);
    free(c_local);
    free(cantidades);
    free(desplazamientos);

    MPI_Finalize();
    return salida;
}
