
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define PI 3.14159265358979323846

int main(void)
{
    long long N = 1000000;
    long long cruces = 0;

    double L = 1.0;
    double D = 1.0;
    double distancia, angulo, pi;

    clock_t inicio, fin;
    double tiempo;

    srand((unsigned int)time(NULL));

    inicio = clock();

    for (long long i = 0; i < N; i++)
    {
        distancia = ((double)rand() / RAND_MAX) * (D / 2.0);
        angulo = ((double)rand() / RAND_MAX) * (PI / 2.0);

        if (distancia <= (L / 2.0) * sin(angulo))
        {
            cruces++;
        }
    }

    if (cruces > 0)
    {
        pi = (2.0 * L * N) / (D * cruces);
    }
    else
    {
        pi = 0.0;
    }

    fin = clock();
    tiempo = (double)(fin - inicio) / CLOCKS_PER_SEC;

    printf("Algoritmo: Needles Serial\n");
    printf("Simulaciones: %lld\n", N);
    printf("Agujas que cruzan: %lld\n", cruces);

    if (cruces > 0)
    {
        printf("PI aproximado: %.10f\n", pi);
    }
    else
    {
        printf("No se pudo calcular PI.\n");
    }

    printf("Tiempo: %.6f segundos\n", tiempo);

    return 0;
}
