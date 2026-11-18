
#include <mpi.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ORDEN_PREDETERMINADO 3
#define LIMITE_PREDETERMINADO 10

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

/* Reserva memoria para vectores locales.
   Se reserva al menos un elemento si no hay filas asignadas. */
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

static void llenar_matriz(int *matriz, size_t orden, int limite)
{
    size_t total = orden * orden;
    size_t i;

    for (i = 0; i < total; ++i)
    {
        matriz[i] = 1 + (int)(siguiente_aleatorio() % (uint32_t)limite);
    }
}

/* Cada proceso calcula exclusivamente las filas que recibió. */
static void multiplicar_filas(const int *a_local,
                              const int *b,
                              int *c_local,
                              size_t filas_local,
                              size_t orden)
{
    size_t i, j, k;

    for (i = 0; i < filas_local; ++i)
    {
        for (j = 0; j < orden; ++j)
        {
            int suma = 0;

            for (k = 0; k < orden; ++k)
            {
                suma += a_local[i * orden + k] *
                        b[k * orden + j];
            }

            c_local[i * orden + j] = suma;
        }
    }
}

static void imprimir_matriz(const char *nombre,
                            const int *matriz,
                            size_t orden)
{
    size_t i, j;

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
    int rank, procesos;

    int orden_int = 0;
    int limite = LIMITE_PREDETERMINADO;
    uint32_t semilla = 0;

    size_t orden;
    size_t total;
    size_t filas_local;
    size_t elementos_local;

    int configuracion_ok = 1;
    int memoria_ok;
    int memoria_global_ok;

    int *a = NULL;
    int *b = NULL;
    int *c = NULL;

    int *a_local = NULL;
    int *c_local = NULL;

    int *cantidades = NULL;
    int *desplazamientos = NULL;

    double inicio, tiempo_local, tiempo_maximo = 0.0;

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
    {
        fprintf(stderr, "Error al inicializar MPI.\n");
        return EXIT_FAILURE;
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procesos);

    /* -------------------------------------------------
       1. Validacion de parametros en el proceso raiz.
       ------------------------------------------------- */

    if (rank == 0)
    {

        if (argc < 1 || argc > 4)
        {
            fprintf(stderr,
                    "Uso: %s [TAMANO [LIMITE [SEMILLA]]]\n",
                    argv[0]);

            configuracion_ok = 0;
        }
        else if (argc == 1)
        {

            orden_int = ORDEN_PREDETERMINADO;
            limite = LIMITE_PREDETERMINADO;

            printf("No se recibieron parametros; "
                   "se usaran TAMANO=%d y LIMITE=%d.\n\n",
                   orden_int, limite);
        }
        else
        {

            if (!leer_entero_positivo(argv[1], &orden_int))
            {
                fprintf(stderr,
                        "Error: TAMANO debe ser un entero positivo.\n");

                configuracion_ok = 0;
            }

            if (configuracion_ok && argc >= 3 &&
                !leer_entero_positivo(argv[2], &limite))
            {

                fprintf(stderr,
                        "Error: LIMITE debe ser un entero positivo.\n");

                configuracion_ok = 0;
            }
        }

        if (configuracion_ok)
        {

            size_t n = (size_t)orden_int;

            /* Control de desbordamiento de la matriz. */
            if (n > SIZE_MAX / n)
            {

                fprintf(stderr,
                        "Error: TAMANO demasiado grande.\n");

                configuracion_ok = 0;
            }
            else if (n * n > (size_t)INT_MAX)
            {

                fprintf(stderr,
                        "Error: TAMANO^2 supera el limite "
                        "de elementos permitido por MPI.\n");

                configuracion_ok = 0;
            }

            /* Control del desbordamiento del resultado. */
            if (configuracion_ok &&
                (uint64_t)limite * (uint64_t)limite >
                    (uint64_t)INT_MAX / (uint64_t)n)
            {

                fprintf(stderr,
                        "Error: TAMANO * LIMITE^2 debe ser "
                        "menor o igual que %d.\n",
                        INT_MAX);

                configuracion_ok = 0;
            }
        }

        if (configuracion_ok)
        {

            if (argc == 4)
            {

                int semilla_int;

                if (!leer_entero_positivo(argv[3], &semilla_int))
                {

                    fprintf(stderr,
                            "Error: SEMILLA debe ser "
                            "un entero positivo.\n");

                    configuracion_ok = 0;
                }
                else
                {
                    semilla = (uint32_t)semilla_int;
                }
            }
            else
            {
                semilla = (uint32_t)time(NULL);
            }
        }
    }

    /* -------------------------------------------------
       2. Distribucion de los parametros.
       ------------------------------------------------- */

    MPI_Bcast(&configuracion_ok, 1, MPI_INT, 0,
              MPI_COMM_WORLD);

    if (!configuracion_ok)
    {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(&orden_int, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&limite, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&semilla, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);

    orden = (size_t)orden_int;
    total = orden * orden;

    /* -------------------------------------------------
       3. Calculo de las filas asignadas a cada proceso.
       ------------------------------------------------- */

    {
        size_t base = orden / (size_t)procesos;
        size_t sobrantes = orden % (size_t)procesos;

        filas_local = base +
                      ((size_t)rank < sobrantes ? 1 : 0);
    }

    elementos_local = filas_local * orden;

    /* -------------------------------------------------
       4. Reserva de memoria.
       ------------------------------------------------- */

    b = reservar_matriz(orden);

    a_local = reservar_vector(elementos_local);
    c_local = reservar_vector(elementos_local);

    if (rank == 0)
    {

        a = reservar_matriz(orden);
        c = reservar_matriz(orden);

        cantidades = malloc((size_t)procesos * sizeof(int));
        desplazamientos = malloc((size_t)procesos * sizeof(int));
    }

    memoria_ok = (b != NULL &&
                  a_local != NULL &&
                  c_local != NULL);

    if (rank == 0 &&
        (a == NULL || c == NULL ||
         cantidades == NULL || desplazamientos == NULL))
    {

        memoria_ok = 0;
    }

    /* Todos los procesos deben poder reservar memoria. */
    MPI_Allreduce(&memoria_ok, &memoria_global_ok,
                  1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

    if (!memoria_global_ok)
    {

        if (rank == 0)
        {
            fprintf(stderr,
                    "Error: no fue posible reservar "
                    "memoria en todos los procesos.\n");
        }

        free(a);
        free(b);
        free(c);
        free(a_local);
        free(c_local);
        free(cantidades);
        free(desplazamientos);

        MPI_Finalize();
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------
       5. Generacion de matrices en el proceso raiz.
       ------------------------------------------------- */

    if (rank == 0)
    {

        rng_state = semilla;

        llenar_matriz(a, orden, limite);
        llenar_matriz(b, orden, limite);

        /*
         * Se calcula cuantas filas recibe cada proceso
         * y donde comienzan dentro de la matriz original.
         */

        size_t desplazamiento_filas = 0;

        for (int p = 0; p < procesos; ++p)
        {

            size_t filas =
                orden / (size_t)procesos +
                ((size_t)p < orden % (size_t)procesos ? 1 : 0);

            cantidades[p] = (int)(filas * orden);

            desplazamientos[p] =
                (int)(desplazamiento_filas * orden);

            desplazamiento_filas += filas;
        }
    }

    /* -------------------------------------------------
       6. Distribucion de la matriz B.
       ------------------------------------------------- */

    MPI_Bcast(b, (int)total, MPI_INT, 0, MPI_COMM_WORLD);

    /* -------------------------------------------------
       7. Distribucion de filas de A.
       ------------------------------------------------- */

    MPI_Scatterv(a, cantidades, desplazamientos, MPI_INT,
                 a_local, (int)elementos_local, MPI_INT,
                 0, MPI_COMM_WORLD);

    /* -------------------------------------------------
       8. Multiplicacion paralela.
       ------------------------------------------------- */

    MPI_Barrier(MPI_COMM_WORLD);

    inicio = MPI_Wtime();

    multiplicar_filas(a_local, b, c_local,
                      filas_local, orden);

    tiempo_local = MPI_Wtime() - inicio;

    /* Se obtiene el mayor tiempo de calculo. */
    MPI_Reduce(&tiempo_local, &tiempo_maximo,
               1, MPI_DOUBLE, MPI_MAX,
               0, MPI_COMM_WORLD);

    /* -------------------------------------------------
       9. Recoleccion de los resultados.
       ------------------------------------------------- */

    MPI_Gatherv(c_local, (int)elementos_local, MPI_INT,
                c, cantidades, desplazamientos, MPI_INT,
                0, MPI_COMM_WORLD);

    /* -------------------------------------------------
       10. Presentacion de resultados en el proceso raiz.
       ------------------------------------------------- */

    if (rank == 0)
    {

        printf("\n========== RESULTADOS MPI ==========\n\n");

        printf("Procesos utilizados: %d\n", procesos);
        printf("Orden: %d x %d\n", orden_int, orden_int);
        printf("Valores aleatorios: 1..%d\n", limite);
        printf("Semilla: %u\n", (unsigned int)semilla);

        printf("Tiempo de multiplicacion paralela: "
               "%.6f segundos\n\n",
               tiempo_maximo);

        if (orden <= 10)
        {

            imprimir_matriz("Matriz A", a, orden);
            imprimir_matriz("Matriz B", b, orden);
            imprimir_matriz("Resultado C = A x B", c, orden);
        }
        else
        {

            printf("Las matrices no se muestran porque "
                   "TAMANO es mayor que 10.\n");
        }

        printf("===================================\n");
    }

    /* -------------------------------------------------
       11. Liberacion de memoria y finalizacion.
       ------------------------------------------------- */

    free(a);
    free(b);
    free(c);

    free(a_local);
    free(c_local);

    free(cantidades);
    free(desplazamientos);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
