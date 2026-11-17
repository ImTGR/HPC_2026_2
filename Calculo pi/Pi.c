
#include <mpi.h>
#include <math.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	int n;
	int rank;
	int size;
	int i;
	const double PI_REFERENCE = 3.141592653589793238462643;
	double local_pi;
	double pi;
	double h;
	double sum;
	double x;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	while (1) {
		if (rank == 0) {
			printf("Enter the number of intervals: (0 quits) ");
			fflush(stdout);
			if (scanf("%d", &n) != 1) {
				n = 0;
			}
		}

		MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
		if (n == 0) {
			break;
		}

		h = 1.0 / (double)n;
		sum = 0.0;
		for (i = rank + 1; i <= n; i += size) {
			x = h * ((double)i - 0.5);
			sum += 4.0 / (1.0 + x * x);
		}

		local_pi = h * sum;
		MPI_Reduce(&local_pi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0,
				   MPI_COMM_WORLD);

		if (rank == 0) {
			printf("pi is approximately %.16f, Error is %.16f\n",
				   pi, fabs(pi - PI_REFERENCE));
		}
	}

	MPI_Finalize();
	return 0;
}
