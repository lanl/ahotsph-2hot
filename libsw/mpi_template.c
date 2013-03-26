
  /* This is a template that is included multiple times in MPI_reduce.c */

	    switch(manifest->u.op) {
#ifndef LOC_OPS
	      case MPI_SUM:
		Do_Op(outbuf, +=, inbuf, Type, count);
		break;
	      case MPI_PROD:
		Do_Op(outbuf, *=, inbuf, Type, count);
		break;
	      case MPI_MAX:
		Do_Op(outbuf, 
		      = (*(Type *)outbuf > *(Type *)inbuf) ? *(Type *)outbuf :,
		      inbuf, Type, count);
		break;
	      case MPI_MIN:
		Do_Op(outbuf, 
		      = (*(Type *)outbuf < *(Type *)inbuf) ? *(Type *)outbuf :,
		      inbuf, Type, count);
		break;
#ifdef BIT_OPS
	      case MPI_BAND:
		Do_Op(outbuf, &=, inbuf, Type, count);
		break;
	      case MPI_BOR:
		Do_Op(outbuf, |=, inbuf, Type, count);
		break;
	      case MPI_BXOR:
		Do_Op(outbuf, ^=, inbuf, Type, count);
		break;
	      case MPI_LAND:
		Do_Op(outbuf, = *(Type *)outbuf && , inbuf, Type, count);
		break;
	      case MPI_LOR:
		Do_Op(outbuf, = *(Type *)outbuf || , inbuf, Type, count);
		break;
	      case MPI_LXOR:	/* cripes */
		Do_Op(outbuf, = (!*(Type *)outbuf == !*(Type *)inbuf) ?
		      0 : 1 ||, inbuf, Type, count);
		break;
#endif /*BIT_OPS */
#else /* LOC_OPS */
	      case MPI_MAXLOC:
		Do_Op(outbuf, = (((Type *)outbuf)->x > ((Type *)inbuf)->x) ? 
		      *(Type *)outbuf :, inbuf, Type, count);
		break;
	      case MPI_MINLOC:
		Do_Op(outbuf, = (((Type *)outbuf)->x < ((Type *)inbuf)->x) ? 
		      *(Type *)outbuf :, inbuf, Type, count);
		break;
#endif /* LOC_OPS */
	      default:
		Error("Unknown op in MPI_reduce\n");
	    }











