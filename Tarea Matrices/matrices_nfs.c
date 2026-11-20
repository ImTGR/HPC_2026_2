#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include "matrices_comun.h"

#include <sys/types.h>
#include <unistd.h>

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
