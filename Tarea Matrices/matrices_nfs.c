#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include <mpi.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>

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

/* Deja espacio para "/" y el nombre de cualquier archivo de la ejecucion. */
#define CAPACIDAD_NOMBRE 32
#define CAPACIDAD_DIRECTORIO (PATH_MAX - CAPACIDAD_NOMBRE)

static int crear_directorio(char directorio[CAPACIDAD_DIRECTORIO])
{
    const char *raiz = getenv("MATRICES_NFS_DIR");
    if (raiz == NULL || raiz[0] != '/')
    {
        fprintf(stderr, "Error: define MATRICES_NFS_DIR con una ruta absoluta compartida.\n");
        return 0;
    }

    int longitud = snprintf(directorio, CAPACIDAD_DIRECTORIO, "%s/matrices.XXXXXX", raiz);
    if (longitud < 0 || longitud >= CAPACIDAD_DIRECTORIO)
    {
        fprintf(stderr, "Error: la ruta de MATRICES_NFS_DIR es demasiado larga.\n");
        return 0;
    }
    if (mkdtemp(directorio) == NULL)
    {
        perror(directorio);
        return 0;
    }
    return 1;
}

static FILE *abrir_archivo(const char *directorio, const char *nombre, const char *modo)
{
    char ruta[PATH_MAX];
    snprintf(ruta, sizeof ruta, "%s/%s", directorio, nombre);
    FILE *archivo = fopen(ruta, modo);
    if (archivo == NULL)
    {
        perror(ruta);
    }
    return archivo;
}

static int escribir_bloque(const char *directorio, const char *nombre,
                           const int *datos, size_t cantidad)
{
    FILE *archivo = abrir_archivo(directorio, nombre, "wb");
    if (archivo == NULL)
    {
        return 0;
    }

    size_t escritos = fwrite(datos, sizeof *datos, cantidad, archivo);
    /* Se cierra antes de permitir que otro nodo abra el archivo. */
    if (fclose(archivo) != 0 || escritos != cantidad)
    {
        fprintf(stderr, "Error al escribir o cerrar %s/%s.\n", directorio, nombre);
        return 0;
    }
    return 1;
}

static int leer_bloque(const char *directorio, const char *nombre,
                       size_t inicio, int *datos, size_t cantidad)
{
    FILE *archivo = abrir_archivo(directorio, nombre, "rb");
    if (archivo == NULL)
    {
        return 0;
    }

    off_t desplazamiento = (off_t)inicio * (off_t)sizeof *datos;
    int lectura_ok = fseeko(archivo, desplazamiento, SEEK_SET) == 0 &&
                     fread(datos, sizeof *datos, cantidad, archivo) == cantidad;
    if (fclose(archivo) != 0 || !lectura_ok)
    {
        fprintf(stderr, "Error al leer o cerrar %s/%s.\n", directorio, nombre);
        return 0;
    }
    return 1;
}

static int reunir_resultado(const char *directorio, int *c, size_t orden,
                            int procesos, int bloques)
{
    for (int p = 0; p < bloques; ++p)
    {
        char nombre[CAPACIDAD_NOMBRE];
        snprintf(nombre, sizeof nombre, "C-%d.bin", p);
        Filas filas = repartir_filas(orden, procesos, p);
        if (!leer_bloque(directorio, nombre, 0, c + filas.inicio * orden,
                         filas.cantidad * orden))
        {
            return 0;
        }
    }
    return 1;
}

static void eliminar_archivo(const char *directorio, const char *nombre)
{
    char ruta[PATH_MAX];
    snprintf(ruta, sizeof ruta, "%s/%s", directorio, nombre);
    if (unlink(ruta) != 0 && errno != ENOENT)
    {
        perror(ruta);
    }
}

static int limpiar_directorio(const char *directorio, int bloques)
{
    eliminar_archivo(directorio, "A.bin");
    eliminar_archivo(directorio, "B.bin");
    for (int p = 0; p < bloques; ++p)
    {
        char nombre[CAPACIDAD_NOMBRE];
        snprintf(nombre, sizeof nombre, "C-%d.bin", p);
        eliminar_archivo(directorio, nombre);
    }
    if (rmdir(directorio) != 0)
    {
        perror(directorio);
        return 0;
    }
    return 1;
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
    int bloques = config.orden < procesos ? config.orden : procesos;

    int *a = NULL;
    int *b = reservar_vector(filas.cantidad > 0 ? total : 0);
    int *c = NULL;
    int *a_local = reservar_vector(elementos_local);
    int *c_local = reservar_vector(elementos_local);
    int memoria_ok = b != NULL && a_local != NULL && c_local != NULL;
    if (rank == 0)
    {
        a = reservar_vector(total);
        c = reservar_vector(total);
        memoria_ok = memoria_ok && a != NULL && c != NULL;
    }

    char directorio[CAPACIDAD_DIRECTORIO] = {0};
    int directorio_creado = 0;
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
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio_total = MPI_Wtime();

    int entradas_ok;
    if (rank == 0)
    {
        directorio_creado = crear_directorio(directorio);
        entradas_ok = directorio_creado &&
                      escribir_bloque(directorio, "A.bin", a, total) &&
                      escribir_bloque(directorio, "B.bin", b, total);
    }
    MPI_Bcast(&entradas_ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!entradas_ok)
    {
        goto finalizar;
    }
    MPI_Bcast(directorio, (int)sizeof directorio, MPI_CHAR, 0, MPI_COMM_WORLD);

    int lectura_ok = filas.cantidad == 0 ||
                     (leer_bloque(directorio, "A.bin", filas.inicio * orden,
                                  a_local, elementos_local) &&
                      leer_bloque(directorio, "B.bin", 0, b, total));
    if (!todos_ok(lectura_ok))
    {
        goto finalizar;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio_calculo = MPI_Wtime();
    multiplicar_filas(a_local, b, c_local, filas.cantidad, orden);
    double calculo_local = MPI_Wtime() - inicio_calculo;

    char nombre[CAPACIDAD_NOMBRE];
    snprintf(nombre, sizeof nombre, "C-%d.bin", rank);
    int escritura_ok = filas.cantidad == 0 ||
                       escribir_bloque(directorio, nombre, c_local, elementos_local);
    if (!todos_ok(escritura_ok))
    {
        goto finalizar;
    }

    int resultado_ok;
    double tiempo_total = 0.0;
    if (rank == 0)
    {
        resultado_ok = reunir_resultado(directorio, c, orden, procesos, bloques);
        tiempo_total = MPI_Wtime() - inicio_total;
    }
    MPI_Bcast(&resultado_ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!resultado_ok)
    {
        goto finalizar;
    }

    double tiempo_calculo;
    MPI_Reduce(&calculo_local, &tiempo_calculo, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        presentar_resultados("MPI + NFS", procesos, config, a, b, c,
                             tiempo_calculo, tiempo_total);
    }
    salida = EXIT_SUCCESS;

finalizar:
    if (directorio_creado && !limpiar_directorio(directorio, bloques))
    {
        salida = EXIT_FAILURE;
    }
    MPI_Bcast(&salida, 1, MPI_INT, 0, MPI_COMM_WORLD);

    free(a);
    free(b);
    free(c);
    free(a_local);
    free(c_local);

    MPI_Finalize();
    return salida;
}
