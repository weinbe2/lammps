#!/usr/bin/env python
# ----------------------------------------------------------------------
#   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
#   https://www.lammps.org/ Sandia National Laboratories
#   LAMMPS Development team: developers@lammps.org
#
#   Copyright (2003) Sandia Corporation.  Under the terms of Contract
#   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
#   certain rights in this software.  This software is distributed under
#   the GNU General Public License.
#
#   See the README file in the top-level LAMMPS directory.
# -------------------------------------------------------------------------

from tabulate import AngleTabulate

################################################################################
import math

def harmonic_force(theta):
    k = 50.0
    thetazero = 120.0
    # the force constant in LAMMPS is in energy per radians^2 so convert from degrees to radians
    deg2rad = math.pi / 180.0
    t = (theta - thetazero) * deg2rad
    f = -2.0 * k * t * deg2rad
    return f

def harmonic_energy(theta):
    k = 50.0
    thetazero = 120.0
    # the force constant in LAMMPS is in energy per radians^2 so convert from degrees to radians
    deg2rad = math.pi / 180.0
    t = (theta - thetazero) * deg2rad
    f = k * t * t
    return f

################################################################################

if __name__ == "__main__":
    atable = AngleTabulate(harmonic_energy, harmonic_force, units='real')
    atable.run('HARM')
