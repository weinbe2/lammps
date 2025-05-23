/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef FIX_CLASS
// clang-format off
FixStyle(UPDATE_SPECIAL_BONDS,FixUpdateSpecialBonds);
// clang-format on
#else

#ifndef LMP_FIX_UPDATE_SPECIAL_BONDS_H
#define LMP_FIX_UPDATE_SPECIAL_BONDS_H

#include "fix.h"

#include <set>
#include <utility>

namespace LAMMPS_NS {

class FixUpdateSpecialBonds : public Fix {
 public:
  FixUpdateSpecialBonds(class LAMMPS *, int, char **);
  int setmask() override;
  void setup(int) override;
  void pre_exchange() override;
  void pre_force(int) override;
  void post_run() override;
  void add_broken_bond(int, int);
  void add_created_bond(int, int);
  void write_restart(FILE *) override;

 protected:
  // Create array to store bonds broken this timestep (new)
  // and a set for those broken since the last neighbor list build
  std::vector<std::pair<tagint, tagint>> new_broken_pairs;
  std::set<std::pair<tagint, tagint>> broken_pairs;

  // Create arrays to store newly created this timestep (new)
  // and a set for those created since the last neighbor list build
  std::vector<std::pair<tagint, tagint>> new_created_pairs;
  std::set<std::pair<tagint, tagint>> created_pairs;
};

}    // namespace LAMMPS_NS

#endif
#endif
