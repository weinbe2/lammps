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
   Contributing author: Richard Berger (LANL)
------------------------------------------------------------------------- */

#include "dihedral_multi_harmonic_kokkos.h"

#include "atom_kokkos.h"
#include "atom_masks.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "memory_kokkos.h"
#include "neighbor_kokkos.h"

#include <cmath>

using namespace LAMMPS_NS;

static constexpr double TOLERANCE = 0.05;
static constexpr double SMALL = 0.001;

/* ---------------------------------------------------------------------- */

template<class DeviceType>
DihedralMultiHarmonicKokkos<DeviceType>::DihedralMultiHarmonicKokkos(LAMMPS *lmp) : DihedralMultiHarmonic(lmp)
{
  kokkosable = 1;
  atomKK = (AtomKokkos *) atom;
  neighborKK = (NeighborKokkos *) neighbor;
  execution_space = ExecutionSpaceFromDevice<DeviceType>::space;
  datamask_read = X_MASK | F_MASK | Q_MASK | ENERGY_MASK | VIRIAL_MASK;
  datamask_modify = F_MASK | ENERGY_MASK | VIRIAL_MASK;

  k_warning_flag = DAT::tdual_int_scalar("Dihedral:warning_flag");
  d_warning_flag = k_warning_flag.view<DeviceType>();
  h_warning_flag = k_warning_flag.h_view;

  centroidstressflag = CENTROID_NOTAVAIL;
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
DihedralMultiHarmonicKokkos<DeviceType>::~DihedralMultiHarmonicKokkos()
{
  if (!copymode) {
    memoryKK->destroy_kokkos(k_eatom,eatom);
    memoryKK->destroy_kokkos(k_vatom,vatom);
  }
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void DihedralMultiHarmonicKokkos<DeviceType>::compute(int eflag_in, int vflag_in)
{
  eflag = eflag_in;
  vflag = vflag_in;

  ev_init(eflag,vflag,0);

  // reallocate per-atom arrays if necessary

  if (eflag_atom) {
    if ((int)k_eatom.extent(0) < maxeatom) {
    memoryKK->destroy_kokkos(k_eatom,eatom);
    memoryKK->create_kokkos(k_eatom,eatom,maxeatom,"dihedral:eatom");
    d_eatom = k_eatom.view<DeviceType>();
    } else Kokkos::deep_copy(d_eatom,0.0);
  }
  if (vflag_atom) {
    if ((int)k_vatom.extent(0) < maxvatom) {
    memoryKK->destroy_kokkos(k_vatom,vatom);
    memoryKK->create_kokkos(k_vatom,vatom,maxvatom,"dihedral:vatom");
    d_vatom = k_vatom.view<DeviceType>();
    } else Kokkos::deep_copy(d_vatom,0.0);
  }

  k_a1.template sync<DeviceType>();
  k_a2.template sync<DeviceType>();
  k_a3.template sync<DeviceType>();
  k_a4.template sync<DeviceType>();
  k_a5.template sync<DeviceType>();

  x = atomKK->k_x.view<DeviceType>();
  f = atomKK->k_f.view<DeviceType>();
  neighborKK->k_dihedrallist.template sync<DeviceType>();
  dihedrallist = neighborKK->k_dihedrallist.view<DeviceType>();
  int ndihedrallist = neighborKK->ndihedrallist;
  nlocal = atom->nlocal;
  newton_bond = force->newton_bond;

  h_warning_flag() = 0;
  k_warning_flag.modify_host();
  k_warning_flag.template sync<DeviceType>();

  copymode = 1;

  // loop over neighbors of my atoms

  EV_FLOAT ev;

  if (evflag) {
    if (newton_bond) {
      Kokkos::parallel_reduce(Kokkos::RangePolicy<DeviceType, TagDihedralMultiHarmonicCompute<1,1> >(0,ndihedrallist),*this,ev);
    } else {
      Kokkos::parallel_reduce(Kokkos::RangePolicy<DeviceType, TagDihedralMultiHarmonicCompute<0,1> >(0,ndihedrallist),*this,ev);
    }
  } else {
    if (newton_bond) {
      Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagDihedralMultiHarmonicCompute<1,0> >(0,ndihedrallist),*this);
    } else {
      Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagDihedralMultiHarmonicCompute<0,0> >(0,ndihedrallist),*this);
    }
  }

  // error check

  k_warning_flag.template modify<DeviceType>();
  k_warning_flag.sync_host();
  if (h_warning_flag())
    error->warning(FLERR,"Dihedral problem");

  if (eflag_global) energy += ev.evdwl;
  if (vflag_global) {
    virial[0] += ev.v[0];
    virial[1] += ev.v[1];
    virial[2] += ev.v[2];
    virial[3] += ev.v[3];
    virial[4] += ev.v[4];
    virial[5] += ev.v[5];
  }

  if (eflag_atom) {
    k_eatom.template modify<DeviceType>();
    k_eatom.sync_host();
  }

  if (vflag_atom) {
    k_vatom.template modify<DeviceType>();
    k_vatom.sync_host();
  }

  copymode = 0;
}

template<class DeviceType>
template<int NEWTON_BOND, int EVFLAG>
KOKKOS_INLINE_FUNCTION
void DihedralMultiHarmonicKokkos<DeviceType>::operator()(TagDihedralMultiHarmonicCompute<NEWTON_BOND,EVFLAG>, const int &n, EV_FLOAT& ev) const {

  // The f array is atomic
  Kokkos::View<F_FLOAT*[3], typename DAT::t_f_array::array_layout,typename KKDevice<DeviceType>::value,Kokkos::MemoryTraits<Kokkos::Atomic|Kokkos::Unmanaged> > a_f = f;

  const int i1 = dihedrallist(n,0);
  const int i2 = dihedrallist(n,1);
  const int i3 = dihedrallist(n,2);
  const int i4 = dihedrallist(n,3);
  const int type = dihedrallist(n,4);

  // 1st bond

  const F_FLOAT vb1x = x(i1,0) - x(i2,0);
  const F_FLOAT vb1y = x(i1,1) - x(i2,1);
  const F_FLOAT vb1z = x(i1,2) - x(i2,2);

  // 2nd bond

  const F_FLOAT vb2x = x(i3,0) - x(i2,0);
  const F_FLOAT vb2y = x(i3,1) - x(i2,1);
  const F_FLOAT vb2z = x(i3,2) - x(i2,2);

  const F_FLOAT vb2xm = -vb2x;
  const F_FLOAT vb2ym = -vb2y;
  const F_FLOAT vb2zm = -vb2z;

  // 3rd bond

  const F_FLOAT vb3x = x(i4,0) - x(i3,0);
  const F_FLOAT vb3y = x(i4,1) - x(i3,1);
  const F_FLOAT vb3z = x(i4,2) - x(i3,2);

  // c0 calculation

  const F_FLOAT sb1 = 1.0 / (vb1x * vb1x + vb1y * vb1y + vb1z * vb1z);
  const F_FLOAT sb2 = 1.0 / (vb2x * vb2x + vb2y * vb2y + vb2z * vb2z);
  const F_FLOAT sb3 = 1.0 / (vb3x * vb3x + vb3y * vb3y + vb3z * vb3z);

  const F_FLOAT rb1 = sqrt(sb1);
  const F_FLOAT rb3 = sqrt(sb3);

  F_FLOAT c0 = (vb1x * vb3x + vb1y * vb3y + vb1z * vb3z) * rb1 * rb3;

  // 1st and 2nd angle

  F_FLOAT b1mag2 = vb1x * vb1x + vb1y * vb1y + vb1z * vb1z;
  F_FLOAT b1mag = sqrt(b1mag2);
  F_FLOAT b2mag2 = vb2x * vb2x + vb2y * vb2y + vb2z * vb2z;
  F_FLOAT b2mag = sqrt(b2mag2);
  F_FLOAT b3mag2 = vb3x * vb3x + vb3y * vb3y + vb3z * vb3z;
  F_FLOAT b3mag = sqrt(b3mag2);

  F_FLOAT ctmp = vb1x * vb2x + vb1y * vb2y + vb1z * vb2z;
  F_FLOAT r12c1 = 1.0 / (b1mag * b2mag);
  F_FLOAT c1mag = ctmp * r12c1;

  ctmp = vb2xm * vb3x + vb2ym * vb3y + vb2zm * vb3z;
  F_FLOAT r12c2 = 1.0 / (b2mag * b3mag);
  F_FLOAT c2mag = ctmp * r12c2;

  // cos and sin of 2 angles and final c

  F_FLOAT sin2 = MAX(1.0 - c1mag * c1mag, 0.0);
  F_FLOAT sc1 = sqrt(sin2);
  if (sc1 < SMALL) sc1 = SMALL;
  sc1 = 1.0 / sc1;

  sin2 = MAX(1.0 - c2mag * c2mag, 0.0);
  F_FLOAT sc2 = sqrt(sin2);
  if (sc2 < SMALL) sc2 = SMALL;
  sc2 = 1.0 / sc2;

  F_FLOAT s1 = sc1 * sc1;
  F_FLOAT s2 = sc2 * sc2;
  F_FLOAT s12 = sc1 * sc2;
  F_FLOAT c = (c0 + c1mag * c2mag) * s12;

  // error check

  if ((c > 1.0 + TOLERANCE || c < (-1.0 - TOLERANCE)) && !d_warning_flag())
    d_warning_flag() = 1;

  if (c > 1.0) c = 1.0;
  if (c < -1.0) c = -1.0;

  // force & energy
  // p = sum (i=1,5) a_i * c**(i-1)
  // pd = dp/dc

  F_FLOAT p = d_a1[type] + c * (d_a2[type] + c * (d_a3[type] + c * (d_a4[type] + c * d_a5[type])));
  F_FLOAT pd = d_a2[type] + c * (2.0 * d_a3[type] + c * (3.0 * d_a4[type] + c * 4.0 * d_a5[type]));

  E_FLOAT edihedral = 0.0;
  if (eflag) edihedral = p;

  const F_FLOAT a = pd;
  c = c * a;
  s12 = s12 * a;
  const F_FLOAT a11 = c * sb1 * s1;
  const F_FLOAT a22 = -sb2 * (2.0 * c0 * s12 - c * (s1 + s2));
  const F_FLOAT a33 = c * sb3 * s2;
  const F_FLOAT a12 = -r12c1 * (c1mag * c * s1 + c2mag * s12);
  const F_FLOAT a13 = -rb1 * rb3 * s12;
  const F_FLOAT a23 = r12c2 * (c2mag * c * s2 + c1mag * s12);

  const F_FLOAT sx2 = a12 * vb1x + a22 * vb2x + a23 * vb3x;
  const F_FLOAT sy2 = a12 * vb1y + a22 * vb2y + a23 * vb3y;
  const F_FLOAT sz2 = a12 * vb1z + a22 * vb2z + a23 * vb3z;

  F_FLOAT f1[3],f2[3],f3[3],f4[3];
  f1[0] = a11 * vb1x + a12 * vb2x + a13 * vb3x;
  f1[1] = a11 * vb1y + a12 * vb2y + a13 * vb3y;
  f1[2] = a11 * vb1z + a12 * vb2z + a13 * vb3z;

  f2[0] = -sx2 - f1[0];
  f2[1] = -sy2 - f1[1];
  f2[2] = -sz2 - f1[2];

  f4[0] = a13 * vb1x + a23 * vb2x + a33 * vb3x;
  f4[1] = a13 * vb1y + a23 * vb2y + a33 * vb3y;
  f4[2] = a13 * vb1z + a23 * vb2z + a33 * vb3z;

  f3[0] = sx2 - f4[0];
  f3[1] = sy2 - f4[1];
  f3[2] = sz2 - f4[2];

  // apply force to each of 4 atoms

  if (NEWTON_BOND || i1 < nlocal) {
    a_f(i1,0) += f1[0];
    a_f(i1,1) += f1[1];
    a_f(i1,2) += f1[2];
  }

  if (NEWTON_BOND || i2 < nlocal) {
    a_f(i2,0) += f2[0];
    a_f(i2,1) += f2[1];
    a_f(i2,2) += f2[2];
  }

  if (NEWTON_BOND || i3 < nlocal) {
    a_f(i3,0) += f3[0];
    a_f(i3,1) += f3[1];
    a_f(i3,2) += f3[2];
  }

  if (NEWTON_BOND || i4 < nlocal) {
    a_f(i4,0) += f4[0];
    a_f(i4,1) += f4[1];
    a_f(i4,2) += f4[2];
  }

  if (EVFLAG)
    ev_tally(ev,i1,i2,i3,i4,edihedral,f1,f3,f4,
             vb1x,vb1y,vb1z,vb2x,vb2y,vb2z,vb3x,vb3y,vb3z);
}

template<class DeviceType>
template<int NEWTON_BOND, int EVFLAG>
KOKKOS_INLINE_FUNCTION
void DihedralMultiHarmonicKokkos<DeviceType>::operator()(TagDihedralMultiHarmonicCompute<NEWTON_BOND,EVFLAG>, const int &n) const {
  EV_FLOAT ev;
  this->template operator()<NEWTON_BOND,EVFLAG>(TagDihedralMultiHarmonicCompute<NEWTON_BOND,EVFLAG>(), n, ev);
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void DihedralMultiHarmonicKokkos<DeviceType>::allocate()
{
  DihedralMultiHarmonic::allocate();

  int n = atom->ndihedraltypes;
  k_a1 = DAT::tdual_ffloat_1d("DihedralMultiHarmonic::a1",n+1);
  k_a2 = DAT::tdual_ffloat_1d("DihedralMultiHarmonic::a2",n+1);
  k_a3 = DAT::tdual_ffloat_1d("DihedralMultiHarmonic::a3",n+1);
  k_a4 = DAT::tdual_ffloat_1d("DihedralMultiHarmonic::a4",n+1);
  k_a5 = DAT::tdual_ffloat_1d("DihedralMultiHarmonic::a5",n+1);

  d_a1 = k_a1.template view<DeviceType>();
  d_a2 = k_a2.template view<DeviceType>();
  d_a3 = k_a3.template view<DeviceType>();
  d_a4 = k_a4.template view<DeviceType>();
  d_a5 = k_a5.template view<DeviceType>();
}

/* ----------------------------------------------------------------------
   set coeffs for one type
------------------------------------------------------------------------- */

template<class DeviceType>
void DihedralMultiHarmonicKokkos<DeviceType>::coeff(int narg, char **arg)
{
  DihedralMultiHarmonic::coeff(narg, arg);

  int n = atom->ndihedraltypes;
  for (int i = 1; i <= n; i++) {
    k_a1.h_view[i] = a1[i];
    k_a2.h_view[i] = a2[i];
    k_a3.h_view[i] = a3[i];
    k_a4.h_view[i] = a4[i];
    k_a5.h_view[i] = a5[i];
  }

  k_a1.modify_host();
  k_a2.modify_host();
  k_a3.modify_host();
  k_a4.modify_host();
  k_a5.modify_host();
}

/* ----------------------------------------------------------------------
   proc 0 reads coeffs from restart file, bcasts them
------------------------------------------------------------------------- */

template<class DeviceType>
void DihedralMultiHarmonicKokkos<DeviceType>::read_restart(FILE *fp)
{
  DihedralMultiHarmonic::read_restart(fp);

  int n = atom->ndihedraltypes;
  for (int i = 1; i <= n; i++) {
    k_a1.h_view[i] = a1[i];
    k_a2.h_view[i] = a2[i];
    k_a3.h_view[i] = a3[i];
    k_a4.h_view[i] = a4[i];
    k_a5.h_view[i] = a5[i];
  }

  k_a1.modify_host();
  k_a2.modify_host();
  k_a3.modify_host();
  k_a4.modify_host();
  k_a5.modify_host();
}

/* ----------------------------------------------------------------------
   tally energy and virial into global and per-atom accumulators
   virial = r1F1 + r2F2 + r3F3 + r4F4 = (r1-r2) F1 + (r3-r2) F3 + (r4-r2) F4
          = (r1-r2) F1 + (r3-r2) F3 + (r4-r3 + r3-r2) F4
          = vb1*f1 + vb2*f3 + (vb3+vb2)*f4
------------------------------------------------------------------------- */

template<class DeviceType>
//template<int NEWTON_BOND>
KOKKOS_INLINE_FUNCTION
void DihedralMultiHarmonicKokkos<DeviceType>::ev_tally(EV_FLOAT &ev, const int i1, const int i2, const int i3, const int i4,
                        F_FLOAT &edihedral, F_FLOAT *f1, F_FLOAT *f3, F_FLOAT *f4,
                        const F_FLOAT &vb1x, const F_FLOAT &vb1y, const F_FLOAT &vb1z,
                        const F_FLOAT &vb2x, const F_FLOAT &vb2y, const F_FLOAT &vb2z,
                        const F_FLOAT &vb3x, const F_FLOAT &vb3y, const F_FLOAT &vb3z) const
{
  E_FLOAT edihedralquarter;
  F_FLOAT v[6];

  // The eatom and vatom arrays are atomic
  Kokkos::View<E_FLOAT*, typename DAT::t_efloat_1d::array_layout,typename KKDevice<DeviceType>::value,Kokkos::MemoryTraits<Kokkos::Atomic|Kokkos::Unmanaged> > v_eatom = k_eatom.view<DeviceType>();
  Kokkos::View<F_FLOAT*[6], typename DAT::t_virial_array::array_layout,typename KKDevice<DeviceType>::value,Kokkos::MemoryTraits<Kokkos::Atomic|Kokkos::Unmanaged> > v_vatom = k_vatom.view<DeviceType>();

  if (eflag_either) {
    if (eflag_global) {
      if (newton_bond) ev.evdwl += edihedral;
      else {
        edihedralquarter = 0.25*edihedral;
        if (i1 < nlocal) ev.evdwl += edihedralquarter;
        if (i2 < nlocal) ev.evdwl += edihedralquarter;
        if (i3 < nlocal) ev.evdwl += edihedralquarter;
        if (i4 < nlocal) ev.evdwl += edihedralquarter;
      }
    }
    if (eflag_atom) {
      edihedralquarter = 0.25*edihedral;
      if (newton_bond || i1 < nlocal) v_eatom[i1] += edihedralquarter;
      if (newton_bond || i2 < nlocal) v_eatom[i2] += edihedralquarter;
      if (newton_bond || i3 < nlocal) v_eatom[i3] += edihedralquarter;
      if (newton_bond || i4 < nlocal) v_eatom[i4] += edihedralquarter;
    }
  }

  if (vflag_either) {
    v[0] = vb1x*f1[0] + vb2x*f3[0] + (vb3x+vb2x)*f4[0];
    v[1] = vb1y*f1[1] + vb2y*f3[1] + (vb3y+vb2y)*f4[1];
    v[2] = vb1z*f1[2] + vb2z*f3[2] + (vb3z+vb2z)*f4[2];
    v[3] = vb1x*f1[1] + vb2x*f3[1] + (vb3x+vb2x)*f4[1];
    v[4] = vb1x*f1[2] + vb2x*f3[2] + (vb3x+vb2x)*f4[2];
    v[5] = vb1y*f1[2] + vb2y*f3[2] + (vb3y+vb2y)*f4[2];

    if (vflag_global) {
      if (newton_bond) {
        ev.v[0] += v[0];
        ev.v[1] += v[1];
        ev.v[2] += v[2];
        ev.v[3] += v[3];
        ev.v[4] += v[4];
        ev.v[5] += v[5];
      } else {
        if (i1 < nlocal) {
          ev.v[0] += 0.25*v[0];
          ev.v[1] += 0.25*v[1];
          ev.v[2] += 0.25*v[2];
          ev.v[3] += 0.25*v[3];
          ev.v[4] += 0.25*v[4];
          ev.v[5] += 0.25*v[5];
        }
        if (i2 < nlocal) {
          ev.v[0] += 0.25*v[0];
          ev.v[1] += 0.25*v[1];
          ev.v[2] += 0.25*v[2];
          ev.v[3] += 0.25*v[3];
          ev.v[4] += 0.25*v[4];
          ev.v[5] += 0.25*v[5];
        }
        if (i3 < nlocal) {
          ev.v[0] += 0.25*v[0];
          ev.v[1] += 0.25*v[1];
          ev.v[2] += 0.25*v[2];
          ev.v[3] += 0.25*v[3];
          ev.v[4] += 0.25*v[4];
          ev.v[5] += 0.25*v[5];
        }
        if (i4 < nlocal) {
          ev.v[0] += 0.25*v[0];
          ev.v[1] += 0.25*v[1];
          ev.v[2] += 0.25*v[2];
          ev.v[3] += 0.25*v[3];
          ev.v[4] += 0.25*v[4];
          ev.v[5] += 0.25*v[5];
        }
      }
    }

    if (vflag_atom) {
      if (newton_bond || i1 < nlocal) {
        v_vatom(i1,0) += 0.25*v[0];
        v_vatom(i1,1) += 0.25*v[1];
        v_vatom(i1,2) += 0.25*v[2];
        v_vatom(i1,3) += 0.25*v[3];
        v_vatom(i1,4) += 0.25*v[4];
        v_vatom(i1,5) += 0.25*v[5];
      }
      if (newton_bond || i2 < nlocal) {
        v_vatom(i2,0) += 0.25*v[0];
        v_vatom(i2,1) += 0.25*v[1];
        v_vatom(i2,2) += 0.25*v[2];
        v_vatom(i2,3) += 0.25*v[3];
        v_vatom(i2,4) += 0.25*v[4];
        v_vatom(i2,5) += 0.25*v[5];
      }
      if (newton_bond || i3 < nlocal) {
        v_vatom(i3,0) += 0.25*v[0];
        v_vatom(i3,1) += 0.25*v[1];
        v_vatom(i3,2) += 0.25*v[2];
        v_vatom(i3,3) += 0.25*v[3];
        v_vatom(i3,4) += 0.25*v[4];
        v_vatom(i3,5) += 0.25*v[5];
      }
      if (newton_bond || i4 < nlocal) {
        v_vatom(i4,0) += 0.25*v[0];
        v_vatom(i4,1) += 0.25*v[1];
        v_vatom(i4,2) += 0.25*v[2];
        v_vatom(i4,3) += 0.25*v[3];
        v_vatom(i4,4) += 0.25*v[4];
        v_vatom(i4,5) += 0.25*v[5];
      }
    }
  }
}

/* ---------------------------------------------------------------------- */

namespace LAMMPS_NS {
template class DihedralMultiHarmonicKokkos<LMPDeviceType>;
#ifdef LMP_KOKKOS_GPU
template class DihedralMultiHarmonicKokkos<LMPHostType>;
#endif
}

