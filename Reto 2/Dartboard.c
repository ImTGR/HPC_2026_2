
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    long long N = 1000000;
    long long dentro = 0;
    double x, y, pi;

    clock_t inicio, fin;
    double tiempo;

    srand((unsigned int)time(NULL));

    inicio = clock();

    for (long long i = 0; i < N; i++)
    {
        x = 2.0 * rand() / RAND_MAX - 1.0;
        y = 2.0 * rand() / RAND_MAX - 1.0;

        if (x * x + y * y <= 1.0)
        {
            dentro++;
        }
    }

    pi = 4.0 * (double)dentro / N;

    fin = clock();
    tiempo = (double)(fin - inicio) / CLOCKS_PER_SEC;

    printf("Algoritmo: Dartboard Serial\n");
    printf("Simulaciones: %lld\n", N);
    printf("Puntos dentro: %lld\n", dentro);
    printf("PI aproximado: %.10f\n", pi);
    printf("Tiempo: %.6f segundos\n", tiempo);

    return 0;
}
