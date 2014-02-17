#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"


#define SIZE (1048576*256)       /* read/write size per node in bytes */

main(int argc, char **argv)
{
    int *buf, i, j, procnum, nprocs;
    double stim, read_tim, write_tim, max_read_tim, max_write_tim;
    double read_bw, write_bw;
    FILE *fp;
    char filename[256];
    size_t ret;

    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &procnum);

    buf = (int *) malloc(SIZE);
    sprintf(filename, "%s.%05d", argv[1], procnum);

    stim = MPI_Wtime();
    fp = fopen(filename, "a");
    if (fp == NULL) {
	fprintf(stderr, "open failed\n");
	exit(1);
    }
    ret = fwrite(buf, SIZE, 1, fp);
    if (ret != 1) {
	fprintf(stderr, "write failed\n");
	exit(1);
    }
    fclose(fp);
    write_tim = MPI_Wtime() - stim;
  
    MPI_Barrier(MPI_COMM_WORLD);

    sprintf(filename, "%s.%05d", argv[1], (procnum+16)%nprocs);
    stim = MPI_Wtime();
    fp = fopen(filename, "r");
    if (fp == NULL) {
	fprintf(stderr, "open failed\n");
	exit(1);
    }
    ret = fread(buf, SIZE, 1, fp);
    if (ret != 1) {
	fprintf(stderr, "read failed\n");
	exit(1);
    }
    fclose(fp);
    read_tim = MPI_Wtime() - stim;
  
    MPI_Allreduce(&write_tim, &max_write_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&read_tim, &max_read_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);

    if (procnum == 0) {
	read_bw = ((double)SIZE*nprocs)/(max_read_tim*1000000.0);
	write_bw = ((double)SIZE*nprocs)/(max_write_tim*1000000.0);
	printf("posixio write bandwidth = %.2f Mbytes/sec\n", write_bw);
	printf("posixio read bandwidth  = %.2f Mbytes/sec\n", read_bw);
    }

    free(buf);
    MPI_Finalize();
}

