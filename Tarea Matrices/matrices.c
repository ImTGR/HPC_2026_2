#include "matrices_comun.h"

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
