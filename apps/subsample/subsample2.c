#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include "SDF.h"
#include "SDFread.h"
#include "Malloc.h"
#include "error.h"
#include "singlio.h"
#include "mpmy.h"
#include "timers.h"
#include "randoms.h"
#include "SDFwrite.h"
#include "Msgs.h"
#include "memfile.h"
#include "physics.h"
#include "cosmo.h"
#include "output.h"

SDF *
ReadData(char *name, body **btab, int64_t *gnobj, int *nobj)
{
    int massconf, xconf, yconf, zconf;
    int vxconf, vyconf, vzconf;
    int identconf;
    SDF *sdfp;

    singlPrintf("Reading %s\n", name);
    sdfp = SDFreadf64(NULL, name, (void *)btab, gnobj, nobj, sizeof(body),
		      "mass", offsetof(body, mass), &massconf,
		      "x", offsetof(body, pos[0]), &xconf,
		      "y", offsetof(body, pos[1]), &yconf,
		      "z", offsetof(body, pos[2]), &zconf,
		      "vx", offsetof(body, vel[0]), &vxconf,
		      "vy", offsetof(body, vel[1]), &vyconf,
		      "vz", offsetof(body, vel[2]), &vzconf,
		      "ident", offsetof(body, ident), &identconf,
		      NULL);

    for (int i = 0; i < *nobj; i++) {
	(*btab)[i].phi = 0.0f;
	(*btab)[i].acc[0] = 0.0f;
	(*btab)[i].acc[1] = 0.0f;
	(*btab)[i].acc[2] = 0.0f;
    }
    
    if (massconf == 0) {
	float particle_mass;
	SinglWarning("No \"mass\" in file, assigning from particle_mass in header\n");
	SDFgetfloatOrDie(sdfp, "particle_mass", &particle_mass);
	for (int i = 0; i < *nobj; i++) {
	    (*btab)[i].mass = particle_mass;
	}
    }
    if (xconf==0 || yconf==0 || zconf==0) {
	SinglError("Could not find %s %s %s in data file!\n",
		   (xconf==0)? "x" : "",
		   (yconf==0)? "y" : "",
		   (zconf==0)? "z" : "");
    }
    if (vxconf != vyconf || vxconf != vzconf) {
	SinglError("Missing velocity components!\n");
    }
    if (identconf == 0) {
	SinglError("No \"ident\" in file.\n");
    }
    singlPrintf("Data read, gnobj=%ld\n", *gnobj);
    
    return sdfp;
}

int
main(int argc, char **argv)
{
    MPMY_Init(&argc, &argv);
    if (argc != 5) {
	singlPrintf("usage: %s subfrac seed infile.sdf outbase\n", argv[0]);
	exit(1);
    }
    singlPrintf("Welcome to the machine\n");
    memfile_init(32768);
    Msg_addfile(0, (Msgvfprintf_t)memfile_vfprintf, 0);

    double s = atof(argv[1]);
    int seed = atoi(argv[2]);
    char *infile = argv[3];
    char *outfile = argv[4];

    int nobj;
    int64_t gnobj;
    body *btab;
    SDF *sdfp = ReadData(infile, &btab, &gnobj, &nobj);

    int iter = 0;
    double tpos = 0;
    double tvel = 0;
    int do_cosmology = 1;
    int do_periodic = 1;
    float eps = 0;
    float this_eps_scaled = 0;
    int force_smoothing_type = 4;
    float this_tol = 0;
    float rel_tol = 0;
    float rel_tol0 = 0;
    float R[NDIM] = {};
    int N[NDIM] = {};
    double pe = 0;
    double ke = 0;
    int ic_Nmesh = 0;
    double ic_growthfac = 0;
    cosmology cosmo = {};

    SDFgetintOrDefault(sdfp, "iter",  &iter, 0);
    SDFgetdoubleOrDefault(sdfp, "tpos",  &tpos, 0.0);
    SDFgetdoubleOrDefault(sdfp, "tvel",  &tvel, tpos);
    SDFgetfloatOrDie(sdfp, "epsilon_mscale", &eps);
    SDFgetfloatOrDie(sdfp, "epsilon_scaled", &this_eps_scaled);
    SDFgetintOrDefault(sdfp, "force_smoothing_type", &force_smoothing_type, 0);
    SDFgetfloatOrDie(sdfp, "tolerance", &this_tol);
    SDFgetfloatOrDefault(sdfp, "frac_tolerance", &rel_tol, 0.0);
    SDFgetfloatOrDefault(sdfp, "frac_tolerance0", &rel_tol0, 0.0);
    SDFgetintOrDefault(sdfp, "ic_Nmesh",  &ic_Nmesh, 0);
    SDFgetdoubleOrDie(sdfp, "ic_growthfac",  &ic_growthfac);

    SDFgetfloatOrDie(sdfp, "Rx",  &R[0]);
    SDFgetfloatOrDie(sdfp, "Ry",  &R[1]);
    SDFgetfloatOrDie(sdfp, "Rz",  &R[2]);
    SDFgetintOrDefault(sdfp, "Nx",  &N[0], N[0]);
    SDFgetintOrDefault(sdfp, "Ny",  &N[1], N[1]);
    SDFgetintOrDefault(sdfp, "Nz",  &N[2], N[2]);

    char version_nln[256], compiled_date_nln[256], compiled_time_nln[256];
    SDFgetstring(sdfp, "compiled_version_nln", version_nln, sizeof(version_nln));
    SDFgetstring(sdfp, "compiled_date_nln", compiled_date_nln, sizeof(compiled_date_nln));
    SDFgetstring(sdfp, "compiled_time_nln", compiled_time_nln, sizeof(compiled_time_nln));

    if (do_cosmology) {
	int version;
	SDFgetintOrDefault(sdfp, "version",  &version, 1);
	cosmo.t = tpos;
	SDFgetdoubleOrDefault(sdfp, "H0",  &cosmo.H0, 0.0511365);
	if (version == 1) {
	    SDFgetdoubleOrDefault(sdfp, "Omega0",  &cosmo.Omega0, 1.0);
	    SDFgetdoubleOrDefault(sdfp, "Omega_r",  &cosmo.Omega0_r, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "Omega_m",  &cosmo.Omega0_m, cosmo.Omega0-cosmo.Omega0_r);
	    SDFgetdoubleOrDefault(sdfp, "Omega_de",  &cosmo.Omega0_fld, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "w0",  &cosmo.w0_fld, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "wa",  &cosmo.wa_fld, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "Lambda_prime",  &cosmo.Omega0_lambda, 0.0);
	} else if (version == 2) {
	    SDFgetdoubleOrDie(sdfp, "h_100",  &cosmo.h_100);
	    SDFgetdoubleOrDie(sdfp, "Omega0",  &cosmo.Omega0);
	    SDFgetdoubleOrDie(sdfp, "Omega0_r",  &cosmo.Omega0_r);
	    SDFgetdoubleOrDie(sdfp, "Omega0_m",  &cosmo.Omega0_m);
	    SDFgetdoubleOrDie(sdfp, "Omega0_lambda",  &cosmo.Omega0_lambda);
	    SDFgetdoubleOrDie(sdfp, "Omega0_cdm",  &cosmo.Omega0_cdm);
	    SDFgetdoubleOrDefault(sdfp, "Omega0_ncdm_tot",  &cosmo.Omega0_ncdm_tot, 0.0);
	    SDFgetdoubleOrDie(sdfp, "Omega0_b",  &cosmo.Omega0_b);
	    SDFgetdoubleOrDie(sdfp, "Omega0_g",  &cosmo.Omega0_g);
	    SDFgetdoubleOrDefault(sdfp, "Omega0_ur",  &cosmo.Omega0_ur, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "Omega0_fld",  &cosmo.Omega0_fld, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "w0_fld",  &cosmo.w0_fld, 0.0);
	    SDFgetdoubleOrDefault(sdfp, "wa_fld",  &cosmo.wa_fld, 0.0);
	} else Error("Unsupported file version\n");
	SDFgetdoubleOrDefault(sdfp, "Gnewt", &cosmo.Gnewt, 1.0);
	/* Now we need to get initial values for cosmo.a */
	if (SDFhasname("redshift", sdfp)) {
	    double Z;
	    SDFgetdouble(sdfp, "redshift", &Z);
	    cosmo.a = 1.0/(1.0 + Z);
	} else {
	    SinglError("Sorry.  Tell me the redshift in the data file\n");
	}
	/* The Zel'dovich 'f' factor is only needed for setting initial
	   velocities.  At this point, we don't know if we will be asked
	   to do setpvel, though, so we read it anyway. */
	if (SDFhasname("velocity_fac", sdfp)) {
	    SDFgetdoubleOrDie(sdfp, "velocity_fac", &cosmo.velfac);
	} else {
	    cosmo.velfac = 1.0;
	}
	tbl_init(&cosmo, "cosmology.tbl");
    }
    SDFclose(sdfp);

    double dt = 0.0;
    int write_nfiles = 0;
    int do_output = 1;
    int identsort_output = 0;
    output(outfile, gnobj, nobj, btab, iter, dt, tpos-tvel,
	   &cosmo, tpos, tvel, do_cosmology, do_periodic, 
	   eps, this_eps_scaled, force_smoothing_type, this_tol, 
	   rel_tol, rel_tol0,
	   R, N, write_nfiles, &ke, &pe, do_output, identsort_output,
	   ic_Nmesh, ic_growthfac, s, seed,
	   version_nln, compiled_date_nln, compiled_time_nln);

    if (MPMY_Procnum() == 0) {
	struct stat sb;
	int fd = open(infile, O_RDONLY);
	if (fd != -1) {
	    if (fstat(fd, &sb) == -1) Error("stat failed\n");
	    close(fd);
	    const struct utimbuf ut = {.actime = sb.st_atime, .modtime = sb.st_mtime};
	    utime(outfile, &ut); /* set ctime to origin file */
	}
    }

    MPMY_Finalize();
    exit(0);
}

