.. index:: fix sgcmc

fix sgcmc command
=================

Syntax
""""""

.. code-block:: LAMMPS

   fix ID group-ID sgcmc every_nsteps swap_fraction temperature deltamu ...

* ID, group-ID are documented in :doc:`fix <fix>` command
* sgcmc = style name of this fix command
* every_nsteps = number of MD steps between MC cycles
* swap_fraction = fraction of a full MC cycle carried out at each call (a value of 1.0 will perform as many trial moves as there are atoms)
* temperature = temperature that enters Boltzmann factor in Metropolis criterion (usually the same as MD temperature)
* deltamu = `N-1` chemical potential differences :math:`\mu_1-\mu_2, \ldots, \mu_1-\mu_N` (`N` is the number of atom types)
* Zero or more keyword/value pairs may be appended to fix definition line:

  .. parsed-literal::

     keyword = *variance* or *randseed* or *window_moves* or *window_size*
       *variance* kappa conc1 [conc2] ... [concN]
         kappa = variance constraint parameter
         `c_2`, `c_3`,..., `c_N` = `N-1` target concentration fractions
       *randseed* N
         N = seed for pseudo random number generator
       *window_moves* N
         N = number of times sampling window is moved during one MC cycle
       *window_size* frac
         frac = size of sampling window (must be between 0.5 and 1.0)
       *atomic/energy* yes/no
         yes = use the atomic energy method to calculate energy changes
         no = use the default method to calculate energy changes

Examples
""""""""

.. code-block:: LAMMPS

   fix mc all sgcmc 50 0.1 400.0 -0.55
   fix vc all sgcmc 20 0.2 700.0 -0.7 randseed 324234 variance 2000.0 0.05
   fix 2  all sgcmc 20 0.1 700.0 -0.7 window_moves 20

Description
"""""""""""

.. versionadded:: 22Dec2022

This command allows to carry out parallel hybrid molecular
dynamics/Monte Carlo (MD/MC) simulations using the algorithms described
in :ref:`(Sadigh1) <Sadigh1>`.  Simulations can be carried out in either
the semi-grand canonical (SGC) or variance constrained semi-grand
canonical (VC-SGC) ensemble :ref:`(Sadigh2) <Sadigh2>`. Only atom type
swaps are performed by the SGCMC fix. Relaxations are accounted for by
the molecular dynamics integration steps.

This fix can be used with standard multi-element EAM potentials
(:doc:`pair styles eam/alloy or eam/fs <pair_eam>`)

The SGCMC fix can handle Finnis/Sinclair type EAM potentials where
:math:`\rho(r)` is atom-type specific, such that different elements can
contribute differently to the total electron density at an atomic site
depending on the identity of the element at that atomic site.

------------

If this fix is applied, the regular MD simulation will be interrupted in
defined intervals to carry out a fraction of a Monte Carlo (MC)
cycle. The interval is set using the parameter *every_nsteps* which
determines how many MD integrator steps are taken between subsequent
calls to the MC routine.

It is possible to carry out pure lattice MC simulations by setting
*every_nsteps* to 1 and not defining an integration fix such as NVE,
NPT etc.  In that case, the particles will not move and only the MC
routine will be called to perform atom type swaps.

The parameter *swap_fraction* determines how many MC trial steps are carried
out every time the MC routine is entered. It is measured in units of full MC
cycles where one full cycle, *swap_fraction=1*, corresponds to as many MC
trial steps as there are atoms.

------------

The parameter *temperature* specifies the temperature that is used
to evaluate the Metropolis acceptance criterion. While it usually
should be set to the same value as the MD temperature there are cases
when it can be useful to use two different values for at least part of
the simulation, e.g., to speed up equilibration at low temperatures.

------------

The parameter *deltamu* is used to set the chemical potential differences
in the SGC MC algorithm (see Eq. 16 in :ref:`Sadigh1 <Sadigh1>`).
The `N-1` differences are defined as :math:`\mu_1-\mu_2, \ldots, \mu_1-\mu_N`,
where `N` is the number of atom types.

------------

The variance-constrained SGC MC algorithm is activated if the keyword
*variance* is used. In that case the fix parameter *deltamu* determines
the effective average constraint in the parallel VC-SGC MC algorithm
(parameter :math:`\delta\mu_0` in Eq. (20) of :ref:`Sadigh1
<Sadigh1>`). The parameter *kappa* specifies the variance constraint
(see Eqs. (20-21) in :ref:`Sadigh1 <Sadigh1>`).
The parameter *conc* sets the `N-1` target atomic concentration
fractions (parameter :math:`c_0` in Eqs.  (20-21) of :ref:`Sadigh1 <Sadigh1>`)
:math:`0 \le c_2, \ldots, c_N \le 1`, with
:math:`c_1 = 1 - \Sigma_{i=2}^N c_i`.
When the simulation includes `N` atom types (elements),
`N-1` concentration values must be specified.

------------

There are several technical parameters that can be set via optional flags.

*randseed* is expected to be a positive integer number and is used
to initialize the random number generator on each processor.

*window_size* controls the size of the sampling window in a parallel MC
simulation. The size has to lie between 0.5 and 1.0. Normally, this
parameter should be left unspecified which instructs the code to choose
the optimal window size automatically (see Sect. III.B and Figure 6 in
:ref:`Sadigh1 <Sadigh1>` for details).

The number of times the window is moved during a MC cycle is set using
the parameter *window_moves* (see Sect. III.B in :ref:`Sadigh1
<Sadigh1>` for details).

The *atomic/energy* keyword controls which method is used for calculating
the energy change when atom types are swapped. A value of *no*
uses the default method, see discussion below in Restrictions section.
A value of *yes* uses the atomic energy method,
if the method has been implemented for the LAMMPS energy model,
otherwise LAMMPS will exit with an error message.
So far this has only been implemented for EAM type potentials.

------------

Restart, fix_modify, output, run start/stop, minimize info
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

No information about this fix is written to restart files.

The MC routine keeps track of the global concentration(s) as well as the
number of accepted and rejected trial swaps during each MC step. These
values are provided by the sgcmc fix in the form of a global vector that
can be accessed by various :doc:`output commands <Howto_output>`
components of the vector represent the following quantities:

* 1 = The absolute number of accepted trial swaps during the last MC step
* 2 = The absolute number of rejected trial swaps during the last MC step
* 3 = Current global concentration `c_1` (= number of atoms of type 1 / total number of atoms)
* 4 = Current global concentration `c_2` (= number of atoms of type 2 / total number of atoms)
* ...
* N+2 = Current global concentration `c_N` (= number of atoms of type *N* / total number of atoms)

The vector values calculated by this fix are "intensive".

Restrictions
""""""""""""

This fix is part of the MC package. It is only enabled if LAMMPS was
built with that package.  See the :doc:`Build package <Build_package>`
page for more info.

This fix style requires an :doc:`atom style <atom_style>` with per atom
type masses.

The fix provides three methods for calculating the potential energy
change due to atom type swaps. For EAM type potentials, the default
method is a carefully optimized local energy change calculation that
is part of the source code for this fix. It takes advantage of the
specific computational and communication requirements of EAM. Customizing
the local method to handle other energy models such as Tersoff has been done,
but these cases are not supported in the public LAMMPS code.
For all other LAMMPS energy models, the default method calculates
the *total* potential energy of the system before and after each
atom type swap.  This method does not depend on the details of the
energy model and so is guaranteed to be correct.  It is also
orders of magnitude slower than the custom EAM calculation.
In addition, it can not be used with parallel execution i.e. only
a single MPI process is allowed.
The third method uses the *atomic/energy* keyword described above.
This allows parallel execution and it is also a local calculation,
making it only a bit slower than a fully-optimized local calculation.
So far, this has been implemented for EAM type potentials.
It is straightforward to extend this to other potentials,
requiring adding an atomic energy method to the pair style.

------------

Default
"""""""

The optional parameters default to the following values:

* *randseed* = 324234
* *window_moves* = 8
* *window_size* = automatic
* *atomic/energy* = no

------------

.. _Sadigh1:

**(Sadigh1)** B. Sadigh, P. Erhart, A. Stukowski, A. Caro, E. Martinez, and L. Zepeda-Ruiz, Phys. Rev. B **85**, 184203 (2012)

.. _Sadigh2:

**(Sadigh2)** B. Sadigh and P. Erhart, Phys. Rev. B **86**, 134204 (2012)
