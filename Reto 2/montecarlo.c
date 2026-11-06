/* Buffon (L = D = 1) y Dartboard: comparacion reproducible serial/OpenMP. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#define PI_REF 3.14159265358979323846

static double wall_time(void)
{
#ifdef _WIN32
    /* El omp_get_wtime de algunos MinGW tiene resolucion de milisegundos.
     * QPC permite medir tambien los casos pequenos sin tiempos cero. */
    LARGE_INTEGER counter;
    static LARGE_INTEGER frequency = {0};
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    return omp_get_wtime();
#endif
}

/* Finalizador SplitMix64 sobre un contador: sin estado global ni por hilo.
 * Cada muestra i recibe exactamente las mismas dos palabras, con cualquier
 * numero de hilos/schedule. El desbordamiento uint64_t es modular y definido.
 * Es un generador didactico; no es criptografico ni sustituye validar un RNG
 * para simulaciones cientificas exigentes. */
static inline double uniform01(uint64_t seed, uint64_t word)
{
    uint64_t z = seed + UINT64_C(0x9e3779b97f4a7c15) * (word + 1);
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    z ^= z >> 31;
    return (double)(z >> 11) * 0x1.0p-53; /* [0,1), 53 bits */
}

static inline int dart_hit(double u, double v)
{
    double x = 2.0 * u - 1.0;
    double y = 2.0 * v - 1.0;
    return x * x + y * y <= 1.0;
}

static inline int needle_hit(double u, double v)
{
    /* d = u/2; theta = v*pi/2; d <= sin(theta)/2. */
    return u <= sin(v * (PI_REF / 2.0));
}

/* Los bucles seriales no abren ninguna region paralela. Los otros acumulan
 * contadores enteros privados mediante reduction, sin atomic/critical. */
static uint64_t dartboard(int64_t n, uint64_t seed, int mode)
{
    uint64_t hits = 0;
    if (mode == 0) {
        for (int64_t i = 0; i < n; ++i)
            hits += dart_hit(uniform01(seed, 2 * (uint64_t)i),
                             uniform01(seed, 2 * (uint64_t)i + 1));
    } else if (mode == 1) {
        #pragma omp parallel for default(none) shared(n, seed) schedule(runtime) reduction(+:hits)
        for (int64_t i = 0; i < n; ++i)
            hits += dart_hit(uniform01(seed, 2 * (uint64_t)i),
                             uniform01(seed, 2 * (uint64_t)i + 1));
    } else {
        #pragma omp parallel for simd default(none) shared(n, seed) schedule(runtime) reduction(+:hits)
        for (int64_t i = 0; i < n; ++i)
            hits += dart_hit(uniform01(seed, 2 * (uint64_t)i),
                             uniform01(seed, 2 * (uint64_t)i + 1));
    }
    return hits;
}

static uint64_t needles(int64_t n, uint64_t seed, int mode)
{
    uint64_t hits = 0;
    if (mode == 0) {
        for (int64_t i = 0; i < n; ++i)
            hits += needle_hit(uniform01(seed, 2 * (uint64_t)i),
                               uniform01(seed, 2 * (uint64_t)i + 1));
    } else if (mode == 1) {
        #pragma omp parallel for default(none) shared(n, seed) schedule(runtime) reduction(+:hits)
        for (int64_t i = 0; i < n; ++i)
            hits += needle_hit(uniform01(seed, 2 * (uint64_t)i),
                               uniform01(seed, 2 * (uint64_t)i + 1));
    } else {
        #pragma omp parallel for simd default(none) shared(n, seed) schedule(runtime) reduction(+:hits)
        for (int64_t i = 0; i < n; ++i)
            hits += needle_hit(uniform01(seed, 2 * (uint64_t)i),
                               uniform01(seed, 2 * (uint64_t)i + 1));
    }
    return hits;
}

/* Contraste de memoria controlado. Mismas muestras y predicado, pero guarda
 * dos arreglos de N doubles. Sus tiempos de fase son un perfil instrumentado
 * de ESTA variante, no una atribucion exacta del bucle fusionado. */
static int buffered(int64_t n, uint64_t seed, int needle, uint64_t *hits,
                    double *generation, double *evaluation)
{
    if ((uint64_t)n > SIZE_MAX / (2 * sizeof(double))) return 0;
    double *uv = malloc((size_t)n * 2 * sizeof(double));
    if (!uv) return 0;
    double *v = uv + n;
    double start = wall_time();
    for (int64_t i = 0; i < n; ++i) {
        uv[i] = uniform01(seed, 2 * (uint64_t)i);
        v[i] = uniform01(seed, 2 * (uint64_t)i + 1);
    }
    *generation = wall_time() - start;
    start = wall_time();
    uint64_t count = 0;
    if (needle) {
        for (int64_t i = 0; i < n; ++i) count += needle_hit(uv[i], v[i]);
    } else {
        for (int64_t i = 0; i < n; ++i) count += dart_hit(uv[i], v[i]);
    }
    *evaluation = wall_time() - start;
    *hits = count;
    free(uv);
    return 1;
}

static double process_cpu(void)
{
#ifdef _WIN32
    FILETIME creation, end, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &end, &kernel, &user))
        return -1.0;
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return ((double)k.QuadPart + (double)u.QuadPart) * 1e-7;
#else
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

static uint64_t peak_rss_kib(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof info)) return 0;
    return (uint64_t)(info.PeakWorkingSetSize / 1024);
#else
    struct rusage info;
    if (getrusage(RUSAGE_SELF, &info) != 0) return 0;
#ifdef __APPLE__
    return (uint64_t)info.ru_maxrss / 1024;
#else
    return (uint64_t)info.ru_maxrss;
#endif
#endif
}

static int parse_uint(const char *s, uint64_t *value)
{
    if (!s[0]) return 0;
    for (const char *p = s; *p; ++p) if (*p < '0' || *p > '9') return 0;
    errno = 0;
    char *end;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || *end) return 0;
    *value = (uint64_t)v;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 7) {
        fprintf(stderr, "Uso: %s dartboard|needles serial|omp|simd|buffered N hilos semilla static|dynamic|guided\n", argv[0]);
        return 2;
    }
    int needle = strcmp(argv[1], "needles") == 0;
    if (!needle && strcmp(argv[1], "dartboard")) return 2;
    int mode;
    if (!strcmp(argv[2], "serial")) mode = 0;
    else if (!strcmp(argv[2], "omp")) mode = 1;
    else if (!strcmp(argv[2], "simd")) mode = 2;
    else if (!strcmp(argv[2], "buffered")) mode = 3;
    else return 2;
    uint64_t count, threads, seed;
    if (!parse_uint(argv[3], &count) || count == 0 || count > INT64_MAX ||
        !parse_uint(argv[4], &threads) || threads == 0 || threads > INT_MAX ||
        !parse_uint(argv[5], &seed)) return 2;
    if ((mode == 0 || mode == 3) && threads != 1) {
        fprintf(stderr, "serial y buffered requieren hilos=1\n");
        return 2;
    }
    omp_sched_t schedule;
    if (!strcmp(argv[6], "static")) schedule = omp_sched_static;
    else if (!strcmp(argv[6], "dynamic")) schedule = omp_sched_dynamic;
    else if (!strcmp(argv[6], "guided")) schedule = omp_sched_guided;
    else return 2;
    omp_set_dynamic(0);
    omp_set_num_threads((int)threads);
    /* static,0 divide en bloques; dynamic/guided usan bloques de 1024. */
    omp_set_schedule(schedule, schedule == omp_sched_static ? 0 : 1024);
    int actual_threads = 1;
    /* Diagnostico fuera del intervalo: inicializa/calienta el equipo OpenMP.
     * Las versiones serial/buffered nunca ejecutan esta region. */
    if (mode == 1 || mode == 2) {
        #pragma omp parallel default(none) shared(actual_threads)
        {
            #pragma omp single
            actual_threads = omp_get_num_threads();
        }
    }
    double generation = 0, evaluation = 0;
    double cpu_start = process_cpu();
    double start = wall_time();
    uint64_t hits;
    if (mode == 3) {
        if (!buffered((int64_t)count, seed, needle, &hits, &generation, &evaluation)) {
            fprintf(stderr, "No se pudo reservar memoria para buffered\n");
            return 3;
        }
    } else {
        hits = needle ? needles((int64_t)count, seed, mode)
                      : dartboard((int64_t)count, seed, mode);
    }
    double elapsed = wall_time() - start;
    double cpu_elapsed = process_cpu() - cpu_start;
    double pi = needle ? (hits ? 2.0 * (double)count / (double)hits : NAN)
                       : 4.0 * (double)hits / (double)count;
    /* Error estandar por delta method en Buffon; aproximacion asintotica. */
    double p = (double)hits / (double)count;
    double sigma_p = sqrt(p * (1.0 - p) / (double)count);
    double se = needle ? (hits ? 2.0 * sigma_p / (p * p) : NAN) : 4.0 * sigma_p;
    printf("algorithm,mode,n,threads_requested,threads_actual,seed,schedule,hits,pi,abs_error,se,wall_s,cpu_s,peak_rss_kib,buffer_bytes,generation_s,evaluation_s\n");
    printf("%s,%s,%" PRIu64 ",%" PRIu64 ",%d,%" PRIu64 ",%s,%" PRIu64
           ",%.12f,%.12f,%.12f,%.9f,%.9f,%" PRIu64 ",%" PRIu64 ",%.9f,%.9f\n",
           argv[1], argv[2], count, threads, actual_threads, seed, argv[6], hits,
           pi, fabs(pi - PI_REF), se, elapsed, cpu_elapsed, peak_rss_kib(),
           mode == 3 ? count * 2 * sizeof(double) : UINT64_C(0), generation, evaluation);
    return 0;
}
