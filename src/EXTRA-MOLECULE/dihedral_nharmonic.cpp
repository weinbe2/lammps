// clang-format off
/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Loukas D. Peristeras (Scienomics SARL)
   [ based on dihedral_multi_harmonic.cpp Mathias Puetz (SNL) and friends ]
------------------------------------------------------------------------- */

#include "dihedral_nharmonic.h"

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "memory.h"
#include "neighbor.h"

#include <cmath>

using namespace LAMMPS_NS;

static constexpr double TOLERANCE = 0.05;
static constexpr double SMALL =     0.001;

/* ---------------------------------------------------------------------- */

DihedralNHarmonic::DihedralNHarmonic(LAMMPS *lmp) : Dihedral(lmp)
{
  writedata = 1;
  a = nullptr;
  born_matrix_enable = 1;
}

/* ---------------------------------------------------------------------- */

DihedralNHarmonic::~DihedralNHarmonic()
{
  if (allocated) {
    memory->destroy(setflag);
    for (int i = 1; i <= atom->ndihedraltypes; i++)
      delete [] a[i];
    delete [] a;
    delete [] nterms;
  }
}

/* ---------------------------------------------------------------------- */

void DihedralNHarmonic::compute(int eflag, int vflag)
{
  int i1,i2,i3,i4,n,type;
  double vb1x,vb1y,vb1z,vb2x,vb2y,vb2z,vb3x,vb3y,vb3z,vb2xm,vb2ym,vb2zm;
  double edihedral,f1[3],f2[3],f3[3],f4[3];
  double sb1,sb2,sb3,rb1,rb3,c0,b1mag2,b1mag,b2mag2;
  double b2mag,b3mag2,b3mag,ctmp,c_,r12c1,c1mag,r12c2;
  double c2mag,sc1,sc2,s1,s12,c,p,pd,a11,a22;
  double a33,a12,a13,a23,sx2,sy2,sz2;
  double s2,sin2;

  edihedral = 0.0;
  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int **dihedrallist = neighbor->dihedrallist;
  int ndihedrallist = neighbor->ndihedrallist;
  int nlocal = atom->nlocal;
  int newton_bond = force->newton_bond;

  for (n = 0; n < ndihedrallist; n++) {
    i1 = dihedrallist[n][0];
    i2 = dihedrallist[n][1];
    i3 = dihedrallist[n][2];
    i4 = dihedrallist[n][3];
    type = dihedrallist[n][4];

    // 1st bond

    vb1x = x[i1][0] - x[i2][0];
    vb1y = x[i1][1] - x[i2][1];
    vb1z = x[i1][2] - x[i2][2];

    // 2nd bond

    vb2x = x[i3][0] - x[i2][0];
    vb2y = x[i3][1] - x[i2][1];
    vb2z = x[i3][2] - x[i2][2];

    vb2xm = -vb2x;
    vb2ym = -vb2y;
    vb2zm = -vb2z;

    // 3rd bond

    vb3x = x[i4][0] - x[i3][0];
    vb3y = x[i4][1] - x[i3][1];
    vb3z = x[i4][2] - x[i3][2];

    // c0 calculation

    sb1 = 1.0 / (vb1x*vb1x + vb1y*vb1y + vb1z*vb1z);
    sb2 = 1.0 / (vb2x*vb2x + vb2y*vb2y + vb2z*vb2z);
    sb3 = 1.0 / (vb3x*vb3x + vb3y*vb3y + vb3z*vb3z);

    rb1 = sqrt(sb1);
    rb3 = sqrt(sb3);

    c0 = (vb1x*vb3x + vb1y*vb3y + vb1z*vb3z) * rb1*rb3;

    // 1st and 2nd angle

    b1mag2 = vb1x*vb1x + vb1y*vb1y + vb1z*vb1z;
    b1mag = sqrt(b1mag2);
    b2mag2 = vb2x*vb2x + vb2y*vb2y + vb2z*vb2z;
    b2mag = sqrt(b2mag2);
    b3mag2 = vb3x*vb3x + vb3y*vb3y + vb3z*vb3z;
    b3mag = sqrt(b3mag2);

    ctmp = vb1x*vb2x + vb1y*vb2y + vb1z*vb2z;
    r12c1 = 1.0 / (b1mag*b2mag);
    c1mag = ctmp * r12c1;

    ctmp = vb2xm*vb3x + vb2ym*vb3y + vb2zm*vb3z;
    r12c2 = 1.0 / (b2mag*b3mag);
    c2mag = ctmp * r12c2;

    // cos and sin of 2 angles and final c

    sin2 = MAX(1.0 - c1mag*c1mag,0.0);
    sc1 = sqrt(sin2);
    if (sc1 < SMALL) sc1 = SMALL;
    sc1 = 1.0/sc1;

    sin2 = MAX(1.0 - c2mag*c2mag,0.0);
    sc2 = sqrt(sin2);
    if (sc2 < SMALL) sc2 = SMALL;
    sc2 = 1.0/sc2;

    s1 = sc1 * sc1;
    s2 = sc2 * sc2;
    s12 = sc1 * sc2;
    c = (c0 + c1mag*c2mag) * s12;

    // error check

    if (c > 1.0 + TOLERANCE || c < (-1.0 - TOLERANCE))
      problem(FLERR, i1, i2, i3, i4);

    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;

    // force & energy
    // p = sum (i=1,n) a_i * c**(i-1)
    // pd = dp/dc

    c_ = 1.0;
    p = a[type][0];
    pd = 0.0;
    for (int i = 1; i < nterms[type]; i++) {
      pd += c_ * i * a[type][i];
      c_ *= c;
      p += c_ * a[type][i];
    }

    if (eflag) edihedral = p;

    c = c * pd;
    s12 = s12 * pd;
    a11 = c*sb1*s1;
    a22 = -sb2 * (2.0*c0*s12 - c*(s1+s2));
    a33 = c*sb3*s2;
    a12 = -r12c1*(c1mag*c*s1 + c2mag*s12);
    a13 = -rb1*rb3*s12;
    a23 = r12c2*(c2mag*c*s2 + c1mag*s12);

    sx2  = a12*vb1x + a22*vb2x + a23*vb3x;
    sy2  = a12*vb1y + a22*vb2y + a23*vb3y;
    sz2  = a12*vb1z + a22*vb2z + a23*vb3z;

    f1[0] = a11*vb1x + a12*vb2x + a13*vb3x;
    f1[1] = a11*vb1y + a12*vb2y + a13*vb3y;
    f1[2] = a11*vb1z + a12*vb2z + a13*vb3z;

    f2[0] = -sx2 - f1[0];
    f2[1] = -sy2 - f1[1];
    f2[2] = -sz2 - f1[2];

    f4[0] = a13*vb1x + a23*vb2x + a33*vb3x;
    f4[1] = a13*vb1y + a23*vb2y + a33*vb3y;
    f4[2] = a13*vb1z + a23*vb2z + a33*vb3z;

    f3[0] = sx2 - f4[0];
    f3[1] = sy2 - f4[1];
    f3[2] = sz2 - f4[2];

    // apply force to each of 4 atoms

    if (newton_bond || i1 < nlocal) {
      f[i1][0] += f1[0];
      f[i1][1] += f1[1];
      f[i1][2] += f1[2];
    }

    if (newton_bond || i2 < nlocal) {
      f[i2][0] += f2[0];
      f[i2][1] += f2[1];
      f[i2][2] += f2[2];
    }

    if (newton_bond || i3 < nlocal) {
      f[i3][0] += f3[0];
      f[i3][1] += f3[1];
      f[i3][2] += f3[2];
    }

    if (newton_bond || i4 < nlocal) {
      f[i4][0] += f4[0];
      f[i4][1] += f4[1];
      f[i4][2] += f4[2];
    }

    if (evflag)
      ev_tally(i1,i2,i3,i4,nlocal,newton_bond,edihedral,f1,f3,f4,
               vb1x,vb1y,vb1z,vb2x,vb2y,vb2z,vb3x,vb3y,vb3z);
  }
}

/* ---------------------------------------------------------------------- */

void DihedralNHarmonic::allocate()
{
  allocated = 1;
  int n = atom->ndihedraltypes;

  nterms = new int[n+1];
  a = new double *[n+1];
  for (int i = 1; i <= n; i++) a[i] = nullptr;

  memory->create(setflag,n+1,"dihedral:setflag");
  for (int i = 1; i <= n; i++) setflag[i] = 0;
}

/* ----------------------------------------------------------------------
   set coeffs for one type
------------------------------------------------------------------------- */

void DihedralNHarmonic::coeff(int narg, char **arg)
{
  if (narg < 3) error->all(FLERR,"Incorrect args for dihedral coefficients" + utils::errorurl(21));

  int n = utils::inumeric(FLERR,arg[1],false,lmp);
  if (narg != n + 2)
    error->all(FLERR,"Incorrect args for dihedral coefficients" + utils::errorurl(21));

  if (!allocated) allocate();

  int ilo,ihi;
  utils::bounds(FLERR,arg[0],1,atom->ndihedraltypes,ilo,ihi,error);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    delete[] a[i];
    a[i] = new double [n];
    nterms[i] = n;
    for (int j = 0; j < n; j++) {
      a[i][j] = utils::numeric(FLERR,arg[2+j],false,lmp);
      setflag[i] = 1;
    }
    count++;
  }

  if (count == 0) error->all(FLERR,"Incorrect args for dihedral coefficients" + utils::errorurl(21));
}

/* ----------------------------------------------------------------------
   proc 0 writes out coeffs to restart file
------------------------------------------------------------------------- */

void DihedralNHarmonic::write_restart(FILE *fp)
{
  fwrite(&nterms[1],sizeof(int),atom->ndihedraltypes,fp);
  for (int i = 1; i <= atom->ndihedraltypes; i++)
    fwrite(a[i],sizeof(double),nterms[i],fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads coeffs from restart file, bcasts them
------------------------------------------------------------------------- */

void DihedralNHarmonic::read_restart(FILE *fp)
{
  allocate();

  if (comm->me == 0)
    utils::sfread(FLERR,&nterms[1],sizeof(int),atom->ndihedraltypes,fp,nullptr,error);

  MPI_Bcast(&nterms[1],atom->ndihedraltypes,MPI_INT,0,world);

  // allocate
  for (int i = 1; i <= atom->ndihedraltypes; i++)
    a[i] = new double [nterms[i]];

  if (comm->me == 0) {
    for (int i = 1; i <= atom->ndihedraltypes; i++)
      utils::sfread(FLERR,a[i],sizeof(double),nterms[i],fp,nullptr,error);
  }

  for (int i = 1; i <= atom->ndihedraltypes; i++ )
     MPI_Bcast(a[i],nterms[i],MPI_DOUBLE,0,world);

  for (int i = 1; i <= atom->ndihedraltypes; i++) setflag[i] = 1;
}

/* ----------------------------------------------------------------------
   proc 0 writes to data file
------------------------------------------------------------------------- */

void DihedralNHarmonic::write_data(FILE *fp)
{
  for (int i = 1; i <= atom->ndihedraltypes; i++) {
    fprintf(fp, "%d %d", i, nterms[i]);
    for (int j = 0; j < nterms[i]; j++ )
      fprintf(fp, " %g", a[i][j]);

    fprintf(fp, "\n");
  }

}

/* ----------------------------------------------------------------------*/

void DihedralNHarmonic::born_matrix(int nd, int i1, int i2, int i3, int i4,
                             double &dudih, double &du2dih) {
  int i,type;
  double vb1x,vb1y,vb1z,vb2x,vb2y,vb2z,vb3x,vb3y,vb3z,vb2xm,vb2ym,vb2zm;
  double c,ax,ay,az,bx,by,bz,rasq,rbsq,ra2inv,rb2inv,rabinv;

  int **dihedrallist = neighbor->dihedrallist;
  double **x = atom->x;

  type = dihedrallist[nd][4];

  vb1x = x[i1][0] - x[i2][0];
  vb1y = x[i1][1] - x[i2][1];
  vb1z = x[i1][2] - x[i2][2];

  vb2x = x[i3][0] - x[i2][0];
  vb2y = x[i3][1] - x[i2][1];
  vb2z = x[i3][2] - x[i2][2];

  vb2xm = -vb2x;
  vb2ym = -vb2y;
  vb2zm = -vb2z;

  vb3x = x[i4][0] - x[i3][0];
  vb3y = x[i4][1] - x[i3][1];
  vb3z = x[i4][2] - x[i3][2];

  // c,s calculation

  ax = vb1y*vb2zm - vb1z*vb2ym;
  ay = vb1z*vb2xm - vb1x*vb2zm;
  az = vb1x*vb2ym - vb1y*vb2xm;
  bx = vb3y*vb2zm - vb3z*vb2ym;
  by = vb3z*vb2xm - vb3x*vb2zm;
  bz = vb3x*vb2ym - vb3y*vb2xm;

  rasq = ax*ax + ay*ay + az*az;
  rbsq = bx*bx + by*by + bz*bz;

  ra2inv = rb2inv = 0.0;
  if (rasq > 0) ra2inv = 1.0/rasq;
  if (rbsq > 0) rb2inv = 1.0/rbsq;
  rabinv = sqrt(ra2inv*rb2inv);

  c = (ax*bx + ay*by + az*bz)*rabinv;

  dudih = 0.0;
  du2dih = 0.0;
  for (i = 1; i < nterms[type]; i++) {
    dudih += this->a[type][i]*i*pow(c,i-1);
  }

  for (i = 2; i < nterms[type]; i++) {
    du2dih += this->a[type][i]*i*(i-1)*pow(c, i-2);
  }
}
