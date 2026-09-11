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
        numero <= 0 || numero > INT_MAX) {
        return 0;
    }

    *valor = (int)numero;
    return 1;
}

static int *reservar_matriz(size_t orden)
{
    size_t cantidad;

    /* Evita que orden * orden o cantidad * sizeof(int) se desborden. */
    if (orden > SIZE_MAX / orden) {
        return NULL;
    }
    cantidad = orden * orden;
    if (cantidad > SIZE_MAX / sizeof(int)) {
        return NULL;
    }

    return malloc(cantidad * sizeof(int));
}

static void llenar_matriz(int *matriz, size_t orden, int limite)
{
    size_t total = orden * orden;
    size_t i;

    for (i = 0; i < total; ++i) {
        matriz[i] = 1 + (int)(siguiente_aleatorio() % (uint32_t)limite);
    }
}

static void multiplicar_matrices(const int *a, const int *b, int *c,
                                 size_t orden)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < orden; ++i) {
        for (j = 0; j < orden; ++j) {
            int suma = 0;

            for (k = 0; k < orden; ++k) {
                suma += a[i * orden + k] * b[k * orden + j];
            }
            c[i * orden + j] = suma;
        }
    }
}

static void imprimir_matriz(const char *nombre, const int *matriz,
                            size_t orden)
{
    size_t i;
    size_t j;

    printf("%s:\n", nombre);
    for (i = 0; i < orden; ++i) {
        for (j = 0; j < orden; ++j) {
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
    size_t orden;
    uint32_t semilla;
    int *a = NULL;
    int *b = NULL;
    int *c = NULL;
    clock_t inicio;
    clock_t fin;

    if (argc != 1 && argc != 3 && argc != 4) {
        fprintf(stderr, "Uso: %s [TAMANO LIMITE [SEMILLA]]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 1) {
        orden_int = ORDEN_PREDETERMINADO;
        limite = LIMITE_PREDETERMINADO;
        printf("No se recibieron parametros; se usaran TAMANO=%d y LIMITE=%d.\n\n",
               orden_int, limite);
    } else {
        if (!leer_entero_positivo(argv[1], &orden_int) ||
            !leer_entero_positivo(argv[2], &limite)) {
            fprintf(stderr,
                    "Error: TAMANO y LIMITE deben ser enteros positivos.\n");
            return EXIT_FAILURE;
        }
    }

    orden = (size_t)orden_int;

    /*
     * El mayor resultado posible es orden * limite^2. Se compara mediante
     * division para que ni siquiera la propia comprobacion se desborde.
     */
    if ((uint64_t)limite * (uint64_t)limite >
        (uint64_t)INT_MAX / (uint64_t)orden) {
        fprintf(stderr,
                "Error: TAMANO * LIMITE^2 debe ser menor o igual que %d "
                "para evitar desbordamiento.\n",
                INT_MAX);
        return EXIT_FAILURE;
    }

    if (argc == 4) {
        int semilla_int;

        if (!leer_entero_positivo(argv[3], &semilla_int)) {
            fprintf(stderr, "Error: SEMILLA debe ser un entero positivo.\n");
            return EXIT_FAILURE;
        }
        semilla = (uint32_t)semilla_int;
    } else {
        semilla = (uint32_t)time(NULL);
    }
    rng_state = semilla;

    a = reservar_matriz(orden);
    b = reservar_matriz(orden);
    c = reservar_matriz(orden);
    if (a == NULL || b == NULL || c == NULL) {
        fprintf(stderr, "Error: no fue posible reservar memoria para las matrices.\n");
        free(a);
        free(b);
        free(c);
        return EXIT_FAILURE;
    }

    llenar_matriz(a, orden, limite);
    llenar_matriz(b, orden, limite);

    inicio = clock();
    multiplicar_matrices(a, b, c, orden);
    fin = clock();

    printf("Orden: %d x %d\n", orden_int, orden_int);
    printf("Valores aleatorios: 1..%d\n", limite);
    printf("Semilla: %u\n", semilla);
    printf("Tiempo de multiplicacion: %.6f segundos\n\n",
           (double)(fin - inicio) / CLOCKS_PER_SEC);

    if (orden <= 10) {
        imprimir_matriz("Matriz A", a, orden);
        imprimir_matriz("Matriz B", b, orden);
        imprimir_matriz("Resultado C = A x B", c, orden);
    } else {
        printf("Las matrices no se muestran porque TAMANO es mayor que 10.\n");
    }

    free(a);
    free(b);
    free(c);
    return EXIT_SUCCESS;
}
