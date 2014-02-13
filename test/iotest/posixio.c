#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

/* gcc -O -o posixio posixio.c -I$MPI_ROOT/include -L$MPI_ROOT/lib -L$MPI_ROOT/lib64 -lmpi */

/* A simple performance test. The file name is taken as a 
   command-line argument. */

#define SIZE (1048576*512)       /* read/write size per node in bytes */

main(int argc, char **argv)
{
    int *buf, i, j, procnum, nprocs;
    double stim, read_tim, write_tim, new_read_tim, new_write_tim;
    double min_read_tim=10000000.0, min_write_tim=10000000.0, read_bw, write_bw;
    FILE *fp;
    char filename[256];
    size_t ret;

    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &procnum);

    buf = (int *) malloc(SIZE);
    sprintf(filename, "%s.%05d", argv[1], procnum);

    stim = MPI_Wtime();
    fp = fopen(filename, "w");
    if (fp == NULL) {
	fprintf(stderr, "open failed\n");
	exit(1);
    }
    ret = fwrite(buf, 1, SIZE, fp);
    if (ret != SIZE) {
	fprintf(stderr, "write failed\n");
	exit(1);
    }
    write_tim = MPI_Wtime() - stim;
    fclose(fp);
  
    MPI_Barrier(MPI_COMM_WORLD);

    stim = MPI_Wtime();
    fp = fopen(filename, "r");
    if (fp == NULL) {
	fprintf(stderr, "open failed\n");
	exit(1);
    }
    ret = fread(buf, 1, SIZE, fp);
    if (ret != SIZE) {
	fprintf(stderr, "read failed\n");
	exit(1);
    }
    read_tim = MPI_Wtime() - stim;
    fclose(fp);
  
    MPI_Allreduce(&write_tim, &new_write_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&read_tim, &new_read_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);

    min_read_tim = (new_read_tim < min_read_tim) ? 
	new_read_tim : min_read_tim;
    min_write_tim = (new_write_tim < min_write_tim) ? 
	new_write_tim : min_write_tim;
    
    if (procnum == 0) {
	read_bw = ((double)SIZE*nprocs)/(min_read_tim*1000000.0);
	write_bw = ((double)SIZE*nprocs)/(min_write_tim*1000000.0);
	printf("posixio write bandwidth = %.2f Mbytes/sec\n", write_bw);
	printf("posixio read bandwidth  = %.2f Mbytes/sec\n", read_bw);
    }

    free(buf);
    MPI_Finalize();
}

