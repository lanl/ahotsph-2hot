/*
 * © 2026. Triad National Security, LLC. All rights reserved.
 * This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare. derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

/* A simple performance test. The file name is taken as a                                                
   command-line argument. */

#define SIZE (1048576*128)       /* read/write size per node in bytes */

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

    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_CREATE |
                  MPI_MODE_WRONLY | MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fh);
    offset = (MPI_Offset)procnum*SIZE;
#if 0
    MPI_File_seek(fh, offset, MPI_SEEK_SET);
#else
    MPI_File_set_view(fh, offset, MPI_BYTE, MPI_BYTE, "native", info);
#endif

    MPI_Barrier(MPI_COMM_WORLD);
    stim = MPI_Wtime();
    MPI_File_write(fh, buf, SIZE, MPI_BYTE, &status);
    write_tim = MPI_Wtime() - stim;

    MPI_File_close(&fh);


    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Allreduce(&write_tim, &new_write_tim, 1, MPI_DOUBLE, MPI_MAX,
                  MPI_COMM_WORLD);

    min_write_tim = (new_write_tim < min_write_tim) ?
        new_write_tim : min_write_tim;

    if (procnum == 0) {
        write_bw = ((double)SIZE*nprocs)/(min_write_tim*1000000.0);
        printf("Write bandwidth without file sync = %f Mbytes/sec\n", write_bw);
	fflush(stdout);
    }

    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY |
                  MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fh);
    offset = (MPI_Offset)procnum*SIZE;
#if 0
    MPI_File_seek(fh, offset, MPI_SEEK_SET);
#else
    MPI_File_set_view(fh, offset, MPI_BYTE, MPI_BYTE, "native", info);
#endif

    MPI_Barrier(MPI_COMM_WORLD);
    stim = MPI_Wtime();
    MPI_File_read(fh, buf, SIZE, MPI_BYTE, &status);
    read_tim = MPI_Wtime() - stim;

    MPI_File_close(&fh);

    MPI_Allreduce(&read_tim, &new_read_tim, 1, MPI_DOUBLE, MPI_MAX,
                  MPI_COMM_WORLD);

    min_read_tim = (new_read_tim < min_read_tim) ?
	new_read_tim : min_read_tim;

    if (procnum == 0) {
        read_bw = ((double)SIZE*nprocs)/(min_read_tim*1000000.0);
        printf("Read bandwidth without prior file sync = %f Mbytes/sec\n", read_bw);
    }

    free(buf);
    MPI_Finalize();
}

