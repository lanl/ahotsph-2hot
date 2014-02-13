#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

/* gcc -O -o mpiio mpiio.c -I$MPI_ROOT/include -L$MPI_ROOT/lib -L$MPI_ROOT/lib64 -lmpi */

/* A simple performance test. The file name is taken as a 
   command-line argument. */

#define SIZE (1048576*512)       /* read/write size per node in bytes */

main(int argc, char **argv)
{
    int *buf, i, j, procnum, nprocs;
    MPI_Offset offset;
    double stim, read_tim, write_tim, new_read_tim, new_write_tim;
    double min_read_tim=10000000.0, min_write_tim=10000000.0, read_bw, write_bw;
    MPI_File fh;
    MPI_Status status;
    MPI_Info info = MPI_INFO_NULL;

    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &procnum);

    buf = (int *) malloc(SIZE);

#if 0
    stim = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_CREATE | 
		  MPI_MODE_WRONLY | MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fh);
    offset = (MPI_Offset)procnum*SIZE;
    MPI_File_write_at(fh, offset, buf, SIZE, MPI_BYTE, &status);
    write_tim = MPI_Wtime() - stim;
  
    MPI_File_close(&fh);

    MPI_Barrier(MPI_COMM_WORLD);

    stim = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY | 
		  MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fh);
    offset = (MPI_Offset)procnum*SIZE;
    MPI_File_read_at(fh, offset, buf, SIZE, MPI_BYTE, &status);
    read_tim = MPI_Wtime() - stim;
  
    MPI_File_close(&fh);
  
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
	printf("mpiio write bandwidth = %.2f Mbytes/sec\n", write_bw);
	printf("mpiio read bandwidth  = %.2f Mbytes/sec\n", read_bw);
    }
#endif

    if (procnum == 0) printf("using panfs_concurrent_write\n");

    MPI_Info_create(&info);
    MPI_Info_set(info, "panfs_concurrent_write", "1");

    stim = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_CREATE | 
		  MPI_MODE_WRONLY | MPI_MODE_UNIQUE_OPEN, info, &fh);
    offset = (MPI_Offset)procnum*SIZE;
    MPI_File_write_at(fh, offset, buf, SIZE, MPI_BYTE, &status);
    write_tim = MPI_Wtime() - stim;
  
    MPI_File_close(&fh);

    MPI_Barrier(MPI_COMM_WORLD);

    stim = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY | 
		  MPI_MODE_UNIQUE_OPEN, info, &fh);
    offset = (MPI_Offset)procnum*SIZE;
    MPI_File_read(fh, buf, SIZE, MPI_BYTE, &status);
    read_tim = MPI_Wtime() - stim;
  
    MPI_File_close(&fh);
  
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
	printf("mpiio write bandwidth   = %.2f Mbytes/sec\n", write_bw);
	printf("mpiio read bandwidth    = %.2f Mbytes/sec\n", read_bw);
    }

    free(buf);
    MPI_Finalize();
}

