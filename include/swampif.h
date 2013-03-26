       integer MPI_COMM_WORLD

       parameter (MPI_COMM_WORLD=0)

       integer MPI_SUCCESS, MPI_ERR_OTHER, MPI_ANY_SOURCE, MPI_ANY_TAG

       parameter (MPI_SUCCESS=0)
       parameter (MPI_ERR_OTHER=-1)
       parameter (MPI_ANY_SOURCE=-1)
       parameter (MPI_ANY_TAG=-1)

       integer MPI_STATUS_SIZE, MPI_SOURCE, MPI_TAG, MPI_ERROR

       parameter (MPI_STATUS_SIZE=4)
       parameter (MPI_SOURCE=1)
       parameter (MPI_TAG=2)
       parameter (MPI_ERROR=3)

       integer MPI_REAL, MPI_DOUBLE_PRECISION
       integer MPI_BYTE, MPI_CHARACTER, MPI_INTEGER
       integer MPI_COMPLEX, MPI_DOUBLE_COMPLEX

       parameter (MPI_REAL=0)
       parameter (MPI_DOUBLE_PRECISION=1)
       parameter (MPI_BYTE=3)
       parameter (MPI_CHARACTER=4)
       parameter (MPI_INTEGER=6)
       parameter (MPI_COMPLEX=21)
       parameter (MPI_DOUBLE_COMPLEX=22)

       integer MPI_SUM, MPI_PROD, MPI_MAX, MPI_MIN
       integer MPI_BAND, MPI_BOR, MPI_BXOR, MPI_LAND, MPI_LOR, MPI_LXOR
       integer MPI_MAXLOC, MPI_MINLOC

       parameter (MPI_SUM=0)
       parameter (MPI_PROD=1)
       parameter (MPI_MAX=2)
       parameter (MPI_MIN=3)
       parameter (MPI_BAND=4)
       parameter (MPI_BOR=5)
       parameter (MPI_BXOR=6)
       parameter (MPI_LAND=7)
       parameter (MPI_LOR=8)
       parameter (MPI_LXOR=9)
       parameter (MPI_MAXLOC=10)
       parameter (MPI_MINLOC=11)

       double precision MPI_WTIME
       external MPI_WTIME
