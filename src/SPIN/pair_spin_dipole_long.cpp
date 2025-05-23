// clang-format off
/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/
   LAMMPS development team: developers@lammps.org, Sandia National Laboratories

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------
   Contributing authors: Julien Tranchida (SNL)
                         Stan Moore (SNL)
------------------------------------------------------------------------- */

#include "pair_spin_dipole_long.h"

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "ewald_const.h"
#include "force.h"
#include "info.h"
#include "kspace.h"
#include "math_const.h"
#include "memory.h"
#include "neigh_list.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;
using namespace MathConst;
using namespace EwaldConst;

/* ---------------------------------------------------------------------- */

PairSpinDipoleLong::PairSpinDipoleLong(LAMMPS *lmp) : PairSpin(lmp)
{
  ewaldflag = pppmflag = spinflag = 1;

  hbar = force->hplanck/MY_2PI;                 // eV/(rad.THz)
  mub = 9.274e-4;                               // in A.Ang^2
  mu_0 = 784.15;                                // in eV/Ang/A^2
  mub2mu0 = mub * mub * mu_0 / (4.0*MY_PI);     // in eV.Ang^3
  //mub2mu0 = mub * mub * mu_0 / (4.0*MY_PI);   // in eV
  mub2mu0hbinv = mub2mu0 / hbar;                // in rad.THz

}

/* ----------------------------------------------------------------------
   free all arrays
------------------------------------------------------------------------- */

PairSpinDipoleLong::~PairSpinDipoleLong()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cut_spin_long);
    memory->destroy(cutsq);
    memory->destroy(emag);
  }
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairSpinDipoleLong::settings(int narg, char **arg)
{
  PairSpin::settings(narg,arg);

  cut_spin_long_global = utils::numeric(FLERR,arg[0],false,lmp);

  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++) {
      for (j = i+1; j <= atom->ntypes; j++) {
        if (setflag[i][j]) {
          cut_spin_long[i][j] = cut_spin_long_global;
        }
      }
    }
  }
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairSpinDipoleLong::coeff(int narg, char **arg)
{
  if (!allocated) allocate();

  if (narg != 3)
    error->all(FLERR,"Incorrect args in pair_style command");

  int ilo,ihi,jlo,jhi;
  utils::bounds(FLERR,arg[0],1,atom->ntypes,ilo,ihi,error);
  utils::bounds(FLERR,arg[1],1,atom->ntypes,jlo,jhi,error);

  double spin_long_cut_one = utils::numeric(FLERR,arg[2],false,lmp);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j <= jhi; j++) {
      setflag[i][j] = 1;
      cut_spin_long[i][j] = spin_long_cut_one;
      count++;
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients" + utils::errorurl(21));
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairSpinDipoleLong::init_style()
{
  PairSpin::init_style();

  // ensure use of KSpace long-range solver, set g_ewald

  if (force->kspace == nullptr)
    error->all(FLERR,"Pair style requires a KSpace style");

  g_ewald = force->kspace->g_ewald;
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairSpinDipoleLong::init_one(int i, int j)
{
  if (setflag[i][j] == 0)
    error->all(FLERR, Error::NOLASTLINE,
               "All pair coeffs are not set. Status\n" + Info::get_pair_coeff_status(lmp));

  cut_spin_long[j][i] = cut_spin_long[i][j];

  return cut_spin_long_global;
}

/* ----------------------------------------------------------------------
   extract the larger cutoff if "cut" or "cut_coul"
------------------------------------------------------------------------- */

void *PairSpinDipoleLong::extract(const char *str, int &dim)
{
  if (strcmp(str,"cut") == 0) {
    dim = 0;
    return (void *) &cut_spin_long_global;
  } else if (strcmp(str,"cut_coul") == 0) {
    dim = 0;
    return (void *) &cut_spin_long_global;
  } else if (strcmp(str,"ewald_order") == 0) {
    ewald_order = 0;
    ewald_order |= 1<<1;
    ewald_order |= 1<<3;
    dim = 0;
    return (void *) &ewald_order;
  } else if (strcmp(str,"ewald_mix") == 0) {
    dim = 0;
    return (void *) &mix_flag;
  }
  return nullptr;
}

/* ---------------------------------------------------------------------- */

void PairSpinDipoleLong::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;
  double r,rinv,r2inv,rsq;
  double grij,expm2,t,erfc;
  double evdwl,ecoul;
  double bij[4];
  double xi[3],rij[3],eij[3];
  double spi[4],spj[4];
  double fi[3],fmi[3];
  double local_cut2;
  double pre1,pre2,pre3;
  int *ilist,*jlist,*numneigh,**firstneigh;

  evdwl = ecoul = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;

  double **x = atom->x;
  double **f = atom->f;
  double **fm = atom->fm;
  double **sp = atom->sp;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  int newton_pair = force->newton_pair;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // checking size of emag

  if (nlocal_max < nlocal) {                    // grow emag lists if necessary
    nlocal_max = nlocal;
    memory->grow(emag,nlocal_max,"pair/spin:emag");
  }

  pre1 = 2.0 * g_ewald / MY_PIS;
  pre2 = 4.0 * pow(g_ewald,3.0) / MY_PIS;
  pre3 = 8.0 * pow(g_ewald,5.0) / MY_PIS;

  // computation of the exchange interaction
  // loop over atoms and their neighbors

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    itype = type[i];

    jlist = firstneigh[i];
    jnum = numneigh[i];
    xi[0] = x[i][0];
    xi[1] = x[i][1];
    xi[2] = x[i][2];
    spi[0] = sp[i][0];
    spi[1] = sp[i][1];
    spi[2] = sp[i][2];
    spi[3] = sp[i][3];
    emag[i] = 0.0;

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      jtype = type[j];

      spj[0] = sp[j][0];
      spj[1] = sp[j][1];
      spj[2] = sp[j][2];
      spj[3] = sp[j][3];

      evdwl = 0.0;

      fi[0] = fi[1] = fi[2] = 0.0;
      fmi[0] = fmi[1] = fmi[2] = 0.0;
      bij[0] = bij[1] = bij[2] = bij[3] = 0.0;

      rij[0] = x[j][0] - xi[0];
      rij[1] = x[j][1] - xi[1];
      rij[2] = x[j][2] - xi[2];
      rsq = rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2];
      rinv = 1.0/sqrt(rsq);
      eij[0] = rij[0]*rinv;
      eij[1] = rij[1]*rinv;
      eij[2] = rij[2]*rinv;

      local_cut2 = cut_spin_long[itype][jtype]*cut_spin_long[itype][jtype];

      if (rsq < local_cut2) {
        r2inv = 1.0/rsq;

        r = sqrt(rsq);
        grij = g_ewald * r;
        expm2 = exp(-grij*grij);
        t = 1.0 / (1.0 + EWALD_P*grij);
        erfc = t * (A1+t*(A2+t*(A3+t*(A4+t*A5)))) * expm2;

        bij[0] = erfc * rinv;
        bij[1] = (bij[0] + pre1*expm2) * r2inv;
        bij[2] = (3.0*bij[1] + pre2*expm2) * r2inv;
        bij[3] = (5.0*bij[2] + pre3*expm2) * r2inv;

        compute_long(i,j,eij,bij,fmi,spi,spj);
        if (lattice_flag)
          compute_long_mech(i,j,eij,bij,fmi,spi,spj);

        if (eflag) {
          if (rsq <= local_cut2) {
            evdwl -= (spi[0]*fmi[0] + spi[1]*fmi[1] + spi[2]*fmi[2]);
            evdwl *= 0.5*hbar;
            emag[i] += evdwl;
          }
        } else evdwl = 0.0;

        f[i][0] += fi[0];
        f[i][1] += fi[1];
        f[i][2] += fi[2];
        if (newton_pair || j < nlocal) {
          f[j][0] -= fi[0];
          f[j][1] -= fi[1];
          f[j][2] -= fi[2];
        }
        fm[i][0] += fmi[0];
        fm[i][1] += fmi[1];
        fm[i][2] += fmi[2];

        if (evflag) ev_tally_xyz(i,j,nlocal,newton_pair,
            evdwl,ecoul,fi[0],fi[1],fi[2],rij[0],rij[1],rij[2]);

      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   update the pair interaction fmi acting on the spin ii
------------------------------------------------------------------------- */

void PairSpinDipoleLong::compute_single_pair(int ii, double fmi[3])
{
  int j,jj,jnum,itype,jtype,ntypes;
  int k,locflag;
  int *jlist,*numneigh,**firstneigh;
  double r,rinv,r2inv,rsq,grij,expm2,t,erfc;
  double local_cut2,pre1,pre2,pre3;
  double bij[4],xi[3],rij[3],eij[3],spi[4],spj[4];

  int *type = atom->type;
  double **x = atom->x;
  double **sp = atom->sp;
  double **fm_long = atom->fm_long;

  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // check if interaction applies to type of ii

  itype = type[ii];
  ntypes = atom->ntypes;
  locflag = 0;
  k = 1;
  while (k <= ntypes) {
    if (k <= itype) {
      if (setflag[k][itype] == 1) {
        locflag =1;
        break;
      }
      k++;
    } else if (k > itype) {
      if (setflag[itype][k] == 1) {
        locflag =1;
        break;
      }
      k++;
    } else error->all(FLERR,"Wrong type number");
  }

  // if interaction applies to type ii,
  // locflag = 1 and compute pair interaction

  if (locflag == 1) {

    pre1 = 2.0 * g_ewald / MY_PIS;
    pre2 = 4.0 * pow(g_ewald,3.0) / MY_PIS;
    pre3 = 8.0 * pow(g_ewald,5.0) / MY_PIS;

    // computation of the exchange interaction
    // loop over neighbors of atom i

    xi[0] = x[ii][0];
    xi[1] = x[ii][1];
    xi[2] = x[ii][2];
    spi[0] = sp[ii][0];
    spi[1] = sp[ii][1];
    spi[2] = sp[ii][2];
    spi[3] = sp[ii][3];
    jlist = firstneigh[ii];
    jnum = numneigh[ii];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      jtype = type[j];

      spj[0] = sp[j][0];
      spj[1] = sp[j][1];
      spj[2] = sp[j][2];
      spj[3] = sp[j][3];

      fmi[0] = fmi[1] = fmi[2] = 0.0;
      bij[0] = bij[1] = bij[2] = bij[3] = 0.0;

      rij[0] = x[j][0] - xi[0];
      rij[1] = x[j][1] - xi[1];
      rij[2] = x[j][2] - xi[2];
      rsq = rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2];
      rinv = 1.0/sqrt(rsq);
      eij[0] = rij[0]*rinv;
      eij[1] = rij[1]*rinv;
      eij[2] = rij[2]*rinv;

      local_cut2 = cut_spin_long[itype][jtype]*cut_spin_long[itype][jtype];

      if (rsq < local_cut2) {
        r2inv = 1.0/rsq;

        r = sqrt(rsq);
        grij = g_ewald * r;
        expm2 = exp(-grij*grij);
        t = 1.0 / (1.0 + EWALD_P*grij);
        erfc = t * (A1+t*(A2+t*(A3+t*(A4+t*A5)))) * expm2;

        bij[0] = erfc * rinv;
        bij[1] = (bij[0] + pre1*expm2) * r2inv;
        bij[2] = (3.0*bij[1] + pre2*expm2) * r2inv;
        bij[3] = (5.0*bij[2] + pre3*expm2) * r2inv;

        compute_long(ii,j,eij,bij,fmi,spi,spj);
      }
    }

    // adding the kspace components to fm

    fmi[0] += fm_long[ii][0];
    fmi[1] += fm_long[ii][1];
    fmi[2] += fm_long[ii][2];
  }
}

/* ----------------------------------------------------------------------
   compute dipolar interaction between spins i and j
------------------------------------------------------------------------- */

void PairSpinDipoleLong::compute_long(int /* i */, int /* j */, double eij[3],
    double bij[4], double fmi[3], double spi[4], double spj[4])
{
  double sjeij,pre;
  double b1,b2,gigj;

  gigj = spi[3] * spj[3];
  pre = gigj*mub2mu0hbinv;
  sjeij = spj[0]*eij[0] + spj[1]*eij[1] + spj[2]*eij[2];

  b1 = bij[1];
  b2 = bij[2];

  fmi[0] += pre * (b2 * sjeij * eij[0] - b1 * spj[0]);
  fmi[1] += pre * (b2 * sjeij * eij[1] - b1 * spj[1]);
  fmi[2] += pre * (b2 * sjeij * eij[2] - b1 * spj[2]);
}

/* ----------------------------------------------------------------------
   compute the mechanical force due to the dipolar interaction between
   atom i and atom j
------------------------------------------------------------------------- */

void PairSpinDipoleLong::compute_long_mech(int /* i */, int /* j */, double eij[3],
    double bij[4], double fi[3], double spi[4], double spj[4])
{
  double sisj,sieij,sjeij,b2,b3;
  double g1,g2,g1b2_g2b3,gigj,pre;

  gigj = spi[3] * spj[3];
  pre = 0.5 * gigj*mub2mu0;
  sisj = spi[0]*spj[0] + spi[1]*spj[1] + spi[2]*spj[2];
  sieij = spi[0]*eij[0] + spi[1]*eij[1] + spi[2]*eij[2];
  sjeij = spj[0]*eij[0] + spj[1]*eij[1] + spj[2]*eij[2];

  b2 = bij[2];
  b3 = bij[3];
  g1 = sisj;
  g2 = -sieij*sjeij;
  g1b2_g2b3 = g1*b2 + g2*b3;

  fi[0] += pre * (eij[0] * g1b2_g2b3 + b2 * (sjeij*spi[0] + sieij*spj[0]));
  fi[1] += pre * (eij[1] * g1b2_g2b3 + b2 * (sjeij*spi[1] + sieij*spj[1]));
  fi[2] += pre * (eij[2] * g1b2_g2b3 + b2 * (sjeij*spi[2] + sieij*spj[2]));
}


/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairSpinDipoleLong::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cut_spin_long,n+1,n+1,"pair/spin/long:cut_spin_long");
  memory->create(cutsq,n+1,n+1,"pair/spin/long:cutsq");
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinDipoleLong::write_restart(FILE *fp)
{
  write_restart_settings(fp);

  int i,j;
  for (i = 1; i <= atom->ntypes; i++) {
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        fwrite(&cut_spin_long[i][j],sizeof(int),1,fp);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinDipoleLong::read_restart(FILE *fp)
{
  read_restart_settings(fp);

  allocate();

  int i,j;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++) {
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) utils::sfread(FLERR,&setflag[i][j],sizeof(int),1,fp,nullptr,error);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
      if (setflag[i][j]) {
        if (me == 0) {
          utils::sfread(FLERR,&cut_spin_long[i][j],sizeof(int),1,fp,nullptr,error);
        }
        MPI_Bcast(&cut_spin_long[i][j],1,MPI_DOUBLE,0,world);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinDipoleLong::write_restart_settings(FILE *fp)
{
  fwrite(&cut_spin_long_global,sizeof(double),1,fp);
  fwrite(&mix_flag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinDipoleLong::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    utils::sfread(FLERR,&cut_spin_long_global,sizeof(double),1,fp,nullptr,error);
    utils::sfread(FLERR,&mix_flag,sizeof(int),1,fp,nullptr,error);
  }
  MPI_Bcast(&cut_spin_long_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world);
}
