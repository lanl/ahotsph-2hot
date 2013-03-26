
  /* This is a template that is included multiple times in mpmy_combine.c */

	    switch(manifest->u.op) {
	      case MPMY_SUM:
		Do_Op(outbuf, +=, inbuf, Type, count);
		break;
	      case MPMY_PROD:
		Do_Op(outbuf, *=, inbuf, Type, count);
		break;
	      case MPMY_MAX:
		Do_Op(outbuf, 
		      = (*(Type *)outbuf > *(Type *)inbuf) ? *(Type *)outbuf :,
		      inbuf, Type, count);
		break;
	      case MPMY_MIN:
		Do_Op(outbuf, 
		      = (*(Type *)outbuf < *(Type *)inbuf) ? *(Type *)outbuf :,
		      inbuf, Type, count);
		break;
#ifdef BIT_OPS
	      case MPMY_BAND:
		Do_Op(outbuf, &=, inbuf, Type, count);
		break;
	      case MPMY_BOR:
		Do_Op(outbuf, |=, inbuf, Type, count);
		break;
	      case MPMY_BXOR:
		Do_Op(outbuf, ^=, inbuf, Type, count);
		break;
#endif
	      default:
		Error("Unknown op in mpmy_combine\n");
	    }
