#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

/* cc -O -o mpiio mpiio.c -I$MPI_ROOT/include -L$MPI_ROOT/lib -lmpi */

/* A simple performance test. The file name is taken as a 
   command-line argument. */

#define SIZE (1048576*512)       /* read/write size per node in bytes */

main(int argc, char **argv)
{
    int *buf, i, j, procnum, nprocs;
    MPI_Offset offset;
    double stim, read_tim, write_tim, openr_tim, openw_tim;
    double max_openw_tim, max_openr_tim;
    double min_openw_tim, min_openr_tim;
    double max_read_tim, max_write_tim;
    double min_read_tim, min_write_tim;
    double max_read_bw, max_write_bw;
    double min_read_bw, min_write_bw;
    MPI_File fh;
    MPI_Status status;
    MPI_Info info = MPI_INFO_NULL;

    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &procnum);

    buf = (int *) malloc(SIZE);

    stim = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_CREATE | 
		  MPI_MODE_WRONLY | MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fh);
    openw_tim = MPI_Wtime() - stim;
    offset = (MPI_Offset)procnum*SIZE;
    MPI_File_write_at(fh, offset, buf, SIZE, MPI_BYTE, &status);
    write_tim = MPI_Wtime() - stim;
  
    MPI_File_close(&fh);

    MPI_Barrier(MPI_COMM_WORLD);

    stim = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY | 
		  MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fh);
    openw_tim = MPI_Wtime() - stim;
    offset = (MPI_Offset)procnum*SIZE;
    MPI_File_read_at(fh, offset, buf, SIZE, MPI_BYTE, &status);
    read_tim = MPI_Wtime() - stim;
  
    MPI_File_close(&fh);
  
    MPI_Allreduce(&openw_tim, &max_openw_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&write_tim, &max_write_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&openr_tim, &max_openr_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&read_tim, &max_read_tim, 1, MPI_DOUBLE, MPI_MAX,
		  MPI_COMM_WORLD);

    MPI_Allreduce(&openw_tim, &min_openw_tim, 1, MPI_DOUBLE, MPI_MIN,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&write_tim, &min_write_tim, 1, MPI_DOUBLE, MPI_MIN,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&openr_tim, &min_openr_tim, 1, MPI_DOUBLE, MPI_MIN,
		  MPI_COMM_WORLD);
    MPI_Allreduce(&read_tim, &min_read_tim, 1, MPI_DOUBLE, MPI_MIN,
		  MPI_COMM_WORLD);

    if (procnum == 0) {
	max_read_bw = ((double)SIZE*nprocs)/(max_read_tim*1e9);
	max_write_bw = ((double)SIZE*nprocs)/(max_write_tim*1e9);
	min_read_bw = ((double)SIZE*nprocs)/(min_read_tim*1e9);
	min_write_bw = ((double)SIZE*nprocs)/(min_write_tim*1e9);
	printf("mpiio open write time = %.2f %.2f\n", min_openw_tim, max_openw_tim);
	printf("mpiio open read time = %.2f %.2f\n", min_openr_tim, max_openr_tim);
	printf("mpiio write bandwidth = %.2f %.2f Gbytes/sec\n", min_write_bw, max_write_bw);
	printf("mpiio read bandwidth  = %.2f %.2f Gbytes/sec\n", min_read_bw, min_write_bw);
    }

    free(buf);
    MPI_Finalize();
}

