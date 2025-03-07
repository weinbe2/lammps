
.. index:: fix charge/regulation

fix charge/regulation command
=============================

Syntax
""""""

.. code-block:: LAMMPS

    fix ID group-ID charge/regulation cation_type anion_type keyword value(s)

* ID, group-ID are documented in fix command
* charge/regulation = style name of this fix command
* cation_type = atom type of free cations (integer or type label)
* anion_type = atom type of free anions (integer or type label)

* zero or more keyword/value pairs may be appended

  .. parsed-literal::

     keyword = *pH*, *pKa*, *pKb*, *pIp*, *pIm*, *pKs*, *acid_type*, *base_type*, *lunit_nm*, *temp*, *tempfixid*, *nevery*, *nmc*, *rxd*, *seed*, *tag*, *group*, *onlysalt*, *pmcmoves*
     *pH* value = pH of the solution (can be specified as an equal-style variable)
     *pKa* value = acid dissociation constant (in the -log10 representation)
     *pKb* value = base dissociation constant (in the -log10 representation)
     *pIp* value = activity (effective concentration) of free cations (in the -log10 representation)
     *pIm* value = activity (effective concentration) of free anions (in the -log10 representation)
     *pKs* value = solvent self-dissociation constant (in the -log10 representation)
     *acid_type* = atom type of acid groups (integer or type label)
     *base_type*  = atom type of base groups (integer or type label)
     *lunit_nm* value = unit length used by LAMMPS (# in the units of nanometers)
     *temp* value = temperature
     *tempfixid* value = fix ID of temperature thermostat
     *nevery* value = invoke this fix every nevery steps
     *nmc* value = number of charge regulation MC moves to attempt every nevery steps
     *rxd* value = cutoff distance for acid/base reaction
     *seed* value = random # seed (positive integer)
     *tag* value = yes or no (yes: The code assign unique tags to inserted ions; no: The tag of all inserted ions is "0")
     *group* value = group-ID, inserted ions are assigned to group group-ID. Can be used multiple times to assign inserted ions to multiple groups.
     *onlysalt* values = flag charge_cation charge_anion.
        flag = yes or no (yes: the fix performs only ion insertion/deletion, no: perform acid/base dissociation and ion insertion/deletion)
        charge_cation, charge_anion = value of cation/anion charge, must be an integer (only specify if flag = yes)
     *pmcmoves* values = pmcA pmcB pmcI -  MC move fractions for acid ionization (pmcA), base ionization (pmcB) and free ion exchange (pmcI)

Examples
""""""""

.. code-block:: LAMMPS

    fix chareg all charge/regulation 1 2 acid_type 3 base_type 4 pKa 5.0 pKb 6.0 pH 7.0 pIp 3.0 pIm 3.0 nevery 200 nmc 200 seed 123 tempfixid fT
    fix chareg all charge/regulation 1 2 pIp 3 pIm 3 onlysalt yes 2 -1 seed 123 tag yes temp 1.0

    labelmap atom 1 H+ 2 OH-
    fix chareg all charge/regulation H+ OH- pIp 3 pIm 3 onlysalt yes 2 -1 seed 123 tag yes temp 1.0

Description
"""""""""""

This fix performs Monte Carlo (MC) sampling of charge regulation and
exchange of ions with a reservoir as discussed in :ref:`(Curk1) <Curk1>`
and :ref:`(Curk2) <Curk2>`.  The implemented method is largely analogous
to the grand-reaction ensemble method in :ref:`(Landsgesell)
<Landsgesell>`.  The implementation is parallelized, compatible with
existing LAMMPS functionalities, and applicable to any system utilizing
discrete, ionizable groups or surface sites.

Specifically, the fix implements the following three types of MC moves,
which discretely change the charge state of individual particles and
insert ions into the systems: :math:`\mathrm{A} \rightleftharpoons
\mathrm{A}^-+\mathrm{X}^+`, :math:`\mathrm{B} \rightleftharpoons
\mathrm{B}^++\mathrm{X}^-`, and :math:`\emptyset \rightleftharpoons
Z^-\mathrm{X}^{Z^+}+Z^+\mathrm{X}^{-Z^-}`.  In the former two types of
reactions, Monte Carlo moves alter the charge value of specific atoms
(:math:`\mathrm{A}`, :math:`\mathrm{B}`) and simultaneously insert a
counterion to preserve the charge neutrality of the system, modeling the
dissociation/association process.  The last type of reaction performs
grand canonical MC exchange of ion pairs with a (fictitious) reservoir.

In our implementation "acid" refers to particles that can attain charge
:math:`q=\{0,-1\}` and "base" to particles with :math:`q=\{0,1\}`,
whereas the MC exchange of free ions allows any integer charge values of
:math:`{Z^+}` and :math:`{Z^-}`.

Here we provide several practical examples for modeling charge
regulation effects in solvated systems.  An acid ionization reaction
(:math:`\mathrm{A} \rightleftharpoons \mathrm{A}^-+\mathrm{H}^+`) can be
defined via a single line in the input file

.. code-block:: LAMMPS

    fix acid_reaction all charge/regulation 2 3 acid_type 1 pH 7.0 pKa 5.0 pIp 7.0 pIm 7.0

where the fix attempts to charge :math:`\mathrm{A}` (discharge
:math:`\mathrm{A}^-`) to :math:`\mathrm{A}^-` (:math:`\mathrm{A}`) and
insert (delete) a proton (atom type 2). Besides, the fix implements
self-ionization reaction of water :math:`\emptyset \rightleftharpoons
\mathrm{H}^++\mathrm{OH}^-`.

However, this approach is highly inefficient at :math:`\mathrm{pH}
\approx 7` when the concentration of both protons and hydroxyl ions is
low, resulting in a relatively low acceptance rate of MC moves.

A more efficient way is to allow salt ions to participate in ionization
reactions, which can be easily achieved via

.. code-block:: LAMMPS

    fix acid_reaction2 all charge/regulation 4 5 acid_type 1 pH 7.0 pKa 5.0 pIp 2.0 pIm 2.0

where particles of atom type 4 and 5 are the salt cations and anions,
both at activity (effective concentration) of :math:`10^{-2}` mol/l, see
:ref:`(Curk1) <Curk1>` and :ref:`(Landsgesell) <Landsgesell>` for more
details.

We could have simultaneously added a base ionization reaction
(:math:`\mathrm{B} \rightleftharpoons \mathrm{B}^++\mathrm{OH}^-`)

.. code-block:: LAMMPS

    fix acid_base_reaction all charge/regulation 2 3 acid_type 1 base_type 6 pH 7.0 pKa 5.0 pKb 6.0 pIp 7.0 pIm 7.0

where the fix will attempt to charge :math:`\mathrm{B}` (discharge
:math:`\mathrm{B}^+`) to :math:`\mathrm{B}^+` (:math:`\mathrm{B}`) and
insert (delete) a hydroxyl ion :math:`\mathrm{OH}^-` of atom type 3.


Dissociated ions and salt ions can be combined into a single particle
type, which reduces the number of necessary MC moves and increases
sampling performance, see :ref:`(Curk1) <Curk1>`. The
:math:`\mathrm{H}^+` and monovalent salt cation (:math:`\mathrm{S}^+`)
are combined into a single particle type, :math:`\mathrm{X}^+ =
\{\mathrm{H}^+, \mathrm{S}^+\}`. In this case "pIp" refers to the
effective concentration of the combined cation type :math:`\mathrm{X}^+`
and its value is determined by :math:`10^{-\mathrm{pIp}} =
10^{-\mathrm{pH}} + 10^{-\mathrm{pSp}}`, where
:math:`10^{-\mathrm{pSp}}` is the effective concentration of salt
cations. For example, at pH=7 and pSp=6 we would find pIp~5.958 and the
command that performs reactions with combined ions could read,

.. code-block:: LAMMPS

    fix acid_reaction_combined all charge/regulation 2 3 acid_type 1 pH 7.0 pKa 5.0 pIp 5.958 pIm 5.958



If neither the acid or the base type is specified, for example,

.. code-block:: LAMMPS

    fix salt_reaction all charge/regulation 4 5 pIp 2.0 pIm 2.0

the fix simply inserts or deletes an ion pair of a free cation (atom
type 4) and a free anion (atom type 5) as done in a conventional
grand-canonical MC simulation. Multivalent ions can be inserted
(deleted) by using the *onlysalt* keyword.

This fix is compatible with LAMMPS packages such as MOLECULE or
RIGID. The acid and base particles can be part of larger molecules or
rigid bodies. Free ions that are inserted to or deleted from the system
must be defined as single particles (no bonded interactions allowed) and
cannot be part of larger molecules or rigid bodies. If an atom style
with molecule IDs is used, all inserted ions have a molecule ID equal to
zero.

Note that LAMMPS implicitly assumes a constant number of particles
(degrees of freedom). Since using this fix alters the total number of
particles during the simulation, any thermostat used by LAMMPS, such as
NVT or Langevin, must use a dynamic calculation of system
temperature. This can be achieved by specifying a dynamic temperature
compute (e.g. dtemp) and using it with the desired thermostat, e.g. a
Langevin thermostat:

.. code-block:: LAMMPS

    compute dtemp all temp
    compute_modify dtemp dynamic/dof yes
    fix fT all langevin 1.0 1.0 1.0 123
    fix_modify fT temp dtemp

The units of pH, pKa, pKb, pIp, pIm are considered to be in the
standard -log10 representation assuming reference concentration
:math:`\rho_0 = \mathrm{mol}/\mathrm{l}`.  For example, in the dilute
ideal solution limit, the concentration of free cations will be
:math:`c_\mathrm{I} = 10^{-\mathrm{pIp}}\mathrm{mol}/\mathrm{l}`. To
perform the internal unit conversion, the the value of the LAMMPS unit
length must be specified in nanometers via *lunit_nm*. The default value
is set to the Bjerrum length in water at room temperature (0.71 nm),
*lunit_nm* = 0.71.

The temperature used in MC acceptance probability is set by *temp*. This
temperature should be the same as the temperature set by the molecular
dynamics thermostat. For most purposes, it is probably best to use
*tempfixid* keyword which dynamically sets the temperature equal to the
chosen MD thermostat temperature, in the example above we assumed the
thermostat fix-ID is *fT*. The inserted particles attain a random
velocity corresponding to the specified temperature. Using *tempfixid*
overrides any fixed temperature set by *temp*.

The *rxd* keyword can be used to restrict the inserted/deleted
counterions to a specific radial distance from an acid or base particle
that is currently participating in a reaction. This can be used to
simulate more realist reaction dynamics. If *rxd* = 0 or *rxd* > *L* /
2, where *L* is the smallest box dimension, the radial restriction is
automatically turned off and free ion can be inserted or deleted
anywhere in the simulation box.

If the *tag yes* is used, every inserted atom gets a unique tag ID,
otherwise, the tag of every inserted atom is set to 0. *tag yes* might
cause an integer overflow in very long simulations since the tags are
unique to every particle and thus increase with every successful
particle insertion.

The *pmcmoves* keyword sets the relative probability of attempting the
three types of MC moves (reactions): acid charging, base charging, and
ion pair exchange.  The fix only attempts to perform particle charging
MC moves if *acid_type* or *base_type* is defined. Otherwise fix only
performs free ion insertion/deletion. For example, if *acid_type* is not
defined, *pmcA* is automatically set to 0. The vector *pmcmoves* is
automatically normalized, for example, if set to *pmcmoves* 0 0.33 0.33,
the vector would be normalized to [0,0.5,0.5].

The *only_salt* option can be used to perform multivalent
grand-canonical ion-exchange moves. If *only_salt yes* is used, no
charge exchange is performed, only ion insertion/deletion (*pmcmoves* is
set to [0,0,1]), but ions can be multivalent. In the example above, an
MC move would consist of three ion insertion/deletion to preserve the
charge neutrality of the system.

The *group* keyword can be used to add inserted particles to a specific
group-ID. All inserted particles are automatically added to group *all*.


Output
""""""

This fix computes a global vector of length 8, which can be accessed by
various output commands. The vector values are the following global
quantities:

  #. cumulative MC attempts
  #. cumulative MC successes
  #. current # of neutral acid atoms
  #. current # of -1 charged acid atoms
  #. current # of neutral base atoms
  #. current # of +1 charged base atoms
  #. current # of free cations
  #. current # of free anions


Restrictions
""""""""""""

This fix is part of the MC package. It is only enabled if LAMMPS was
built with that package.  See the :doc:`Build package <Build_package>`
page for more info.

The :doc:`atom_style <atom_style>`, used must contain the charge
property and have per atom type masses, for example, the style could be
*charge* or *full*. Only usable for 3D simulations.  Atoms specified as
free ions cannot be part of rigid bodies or molecules and cannot have
bonding interactions.  The scheme is limited to integer charges, any
atoms with non-integer charges will not be considered by the fix.

All interaction potentials used must be continuous, otherwise the MD
integration and the particle exchange MC moves do not correspond to the
same equilibrium ensemble. For example, if an lj/cut pair style is used,
the LJ potential must be shifted so that it vanishes at the cutoff. This
can be easily achieved using the :doc:`pair_modify <pair_modify>`
command, i.e., by using: *pair_modify shift yes*.

.. note::

   Region restrictions are not yet implemented.

Related commands
""""""""""""""""

:doc:`fix gcmc <fix_gcmc>`,
:doc:`fix atom/swap <fix_atom_swap>`

Default
"""""""

pH = 7.0; pKa = 100.0; pKb = 100.0; pIp = 5.0; pIm = 5.0; pKs = 14.0;
acid_type = -1; base_type = -1; lunit_nm = 0.71; temp = 1.0; nevery =
100; nmc = 100; rxd = 0; seed = 0; tag = no; onlysalt = no, pmcmoves =
[1/3, 1/3, 1/3], group-ID = all

----------

.. _Curk1:

**(Curk1)** T. Curk, J. Yuan, and E. Luijten, "Accelerated simulation method for charge regulation effects", JCP 156 (2022).

.. _Curk2:

**(Curk2)** T. Curk and E. Luijten, "Charge-regulation effects in nanoparticle self-assembly", PRL 126 (2021)

.. _Landsgesell:

**(Landsgesell)** J. Landsgesell, P. Hebbeker, O. Rud, R. Lunkad, P. Kosovan, and C. Holm, "Grand-reaction method for simulations of ionization equilibria coupled to ion partitioning", Macromolecules 53, 3007-3020 (2020).
