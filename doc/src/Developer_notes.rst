Notes for developers and code maintainers
-----------------------------------------

This section documents how some of the code functionality within
LAMMPS works at a conceptual level.  Comments on code in source files
typically document what a variable stores, what a small section of
code does, or what a function does and its input/outputs.  The topics
on this page are intended to document code functionality at a higher level.

.. contents:: Available notes

----

Reading and parsing of text and text files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Classes in LAMMPS frequently need to read in additional
data from a file, e.g. potential parameters from a potential file for
manybody potentials.  LAMMPS provides several custom classes and
convenience functions to simplify the process.  They offer the
following benefits:

- better code reuse and fewer lines of code needed to implement reading
  and parsing data from a file
- better detection of format errors, incompatible data, and better error messages
- exit with an error message instead of silently converting only part of the
  text to a number or returning a 0 on unrecognized text and thus reading incorrect values
- re-entrant code through avoiding global static variables (as used by ``strtok()``)
- transparent support for translating unsupported UTF-8 characters to their ASCII equivalents
  (the text-to-value conversion functions **only** accept ASCII characters)

In most cases (e.g. potential files) the same data is needed on all MPI
ranks.  Then it is best to do the reading and parsing only on MPI rank
0, and communicate the data later with one or more ``MPI_Bcast()``
calls.  For reading generic text and potential parameter files the
custom classes :cpp:class:`TextFileReader <LAMMPS_NS::TextFileReader>`
and :cpp:class:`PotentialFileReader <LAMMPS_NS::PotentialFileReader>`
are available. They allow reading the file as individual lines for which
they can return a tokenizer class (see below) for parsing the line.  Or
they can return blocks of numbers as a vector directly.  The
documentation on :ref:`File reader classes <file-reader-classes>`
contains an example for a typical case.

When reading per-atom data, the data on each line of the file usually
needs to include an atom ID so it can be associated with a particular
atom.  In that case the data can be read in multi-line chunks and
broadcast to all MPI ranks with
:cpp:func:`utils::read_lines_from_file()
<LAMMPS_NS::utils::read_lines_from_file>`.  Those chunks are then
split into lines, parsed, and applied only to atoms the MPI rank
"owns".

For splitting a string (incrementally) into words and optionally
converting those to numbers, the :cpp:class:`Tokenizer
<LAMMPS_NS::Tokenizer>` and :cpp:class:`ValueTokenizer
<LAMMPS_NS::ValueTokenizer>` can be used.  Those provide a superset of
the functionality of ``strtok()`` from the C-library and the latter
also includes conversion to different types.  Any errors while
processing the string in those classes will result in an exception,
which can be caught and the error processed as needed.  Unlike the
C-library functions ``atoi()``, ``atof()``, ``strtol()``, or
``strtod()`` the conversion will check if the converted text is a
valid integer or floating point number and will not silently return an
unexpected or incorrect value.  For example, ``atoi()`` will return 12
when converting "12.5", while the ValueTokenizer class will throw an
:cpp:class:`InvalidIntegerException
<LAMMPS_NS::InvalidIntegerException>` if
:cpp:func:`ValueTokenizer::next_int()
<LAMMPS_NS::ValueTokenizer::next_int>` is called on the same string.

.. _request-neighbor-list:

Requesting and accessing neighbor lists
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

LAMMPS uses Verlet-style neighbor lists to avoid having to loop over
*all* pairs of *all* atoms when computing pairwise properties with a
cutoff (e.g. pairwise forces or radial distribution functions).  There
are three main algorithms that can be selected by the :doc:`neighbor
command <neighbor>`: `bin` (the default, uses binning to achieve linear
scaling with system size), `nsq` (without binning, quadratic scaling),
`multi` (with binning, optimized for varying cutoffs or polydisperse
granular particles).  In addition to how the neighbor lists are
constructed a number of different variants of neighbor lists need to be
created (e.g. "full" or "half") for different purposes and styles and
those may be required in every time step ("perpetual") or on some steps
("occasional").

The neighbor list creation is managed by the ``Neighbor`` class.
Individual classes can obtain a neighbor list by creating an instance of
a ``NeighRequest`` class which is stored in a list inside the
``Neighbor`` class.  The ``Neighbor`` class will then analyze the
various requests and apply optimizations where neighbor lists that have
the same settings will be created only once and then copied, or a list
may be constructed by processing a neighbor list from a different
request that is a superset of the requested list.  The neighbor list
build is then :doc:`processed in parallel <Developer_par_neigh>`.

The most commonly required neighbor list is a so-called "half" neighbor
list, where each pair of atoms is listed only once (except when the
:doc:`newton command setting <newton>` for pair is off; in that case
pairs straddling subdomains or periodic boundaries will be listed twice).
Thus these are the default settings when a neighbor list request is created in:

.. code-block:: c++

   void Pair::init_style()
   {
     neighbor->add_request(this);
   }

   void Pair::init_list(int /*id*/, NeighList *ptr)
   {
     list = ptr;
   }

The ``this`` pointer argument is required so the neighbor list code can
access the requesting class instance to store the assembled neighbor
list with that instance by calling its ``init_list()`` member function.
The optional second argument (omitted here) contains a bitmask of flags
that determines the kind of neighbor list requested.  The default value
used here asks for a perpetual "half" neighbor list.

Non-default values of the second argument need to be used to adjust a
neighbor list request to the specific needs of a style.  The :doc:`tersoff
<pair_tersoff>` pair style, for example, needs a "full" neighbor list:

.. code-block:: c++

   void PairTersoff::init_style()
   {
     // [...]
     neighbor->add_request(this, NeighConst::REQ_FULL);
   }

When a pair style supports r-RESPA time integration with different cutoff regions,
the request flag may depend on the corresponding r-RESPA settings. Here is an
example from pair style lj/cut:

.. code-block:: c++

   void PairLJCut::init_style()
   {
     int list_style = NeighConst::REQ_DEFAULT;

     if (update->whichflag == 1 && utils::strmatch(update->integrate_style, "^respa")) {
       auto respa = (Respa *) update->integrate;
       if (respa->level_inner >= 0) list_style = NeighConst::REQ_RESPA_INOUT;
       if (respa->level_middle >= 0) list_style = NeighConst::REQ_RESPA_ALL;
     }
     neighbor->add_request(this, list_style);
     // [...]
   }

Granular pair styles need neighbor lists based on particle sizes and not cutoff
and also may need to store data across timesteps ("history").
For example with:

.. code-block:: c++

   if (use_history) neighbor->add_request(this, NeighConst::REQ_SIZE | NeighConst::REQ_HISTORY);
   else neighbor->add_request(this, NeighConst::REQ_SIZE);

In case a class would need to make multiple neighbor list requests with different
settings, each request can set an id which is then used in the corresponding
``init_list()`` function to assign it to the suitable pointer variable. This is
done for example by the :doc:`pair style meam <pair_meam>`:

.. code-block:: c++

   void PairMEAM::init_style()
   {
   // [...]
     neighbor->add_request(this, NeighConst::REQ_FULL)->set_id(1);
     neighbor->add_request(this)->set_id(2);
   }
   void PairMEAM::init_list(int id, NeighList *ptr)
   {
     if (id == 1) listfull = ptr;
     else if (id == 2) listhalf = ptr;
   }

Fixes may require a neighbor list that is only build occasionally (or
just once) and this can also be indicated by a flag.  As an example here
is the request from the ``FixPeriNeigh`` class which is created
internally by :doc:`Peridynamics pair styles <pair_peri>`:

.. code-block:: c++

   neighbor->add_request(this, NeighConst::REQ_FULL | NeighConst::REQ_OCCASIONAL);

It is also possible to request a neighbor list that uses a different cutoff
than what is usually inferred from the pair style settings (largest cutoff of
all pair styles plus neighbor list skin).  The following is used in the
:doc:`compute rdf <compute_rdf>` command implementation:

.. code-block:: c++

  if (cutflag)
    neighbor->add_request(this, NeighConst::REQ_OCCASIONAL)->set_cutoff(mycutneigh);
  else
    neighbor->add_request(this, NeighConst::REQ_OCCASIONAL);

The neighbor list request function has a slightly different set of arguments
when created by a command style.  In this case the neighbor list is
*always* an occasional neighbor list, so that flag is not needed. However
for printing the neighbor list summary the name of the requesting command
should be set.  Below is the request from the :doc:`delete atoms <delete_atoms>`
command:

.. code-block:: c++

   neighbor->add_request(this, "delete_atoms", NeighConst::REQ_FULL);


Errors, warnings, and informational messages
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

LAMMPS has specialized functionality to handle errors (which should
terminate LAMMPS), warning messages (which should indicate possible
problems *without* terminating LAMMPS), and informational text for
messages about the progress and chosen settings.  We *strongly*
encourage using these facilities and to *stay away* from using
``printf()`` or ``fprintf()`` or ``std::cout`` or ``std::cerr`` and
calling ``MPI_Abort()`` or ``exit()`` directly.  Warnings and
informational messages should be printed only on MPI rank 0 to avoid
flooding the output when running in parallel with many MPI processes.

**Errors**

When LAMMPS encounters an error, for example a syntax error in the
input, then a suitable error message should be printed giving a brief,
one line remark about the reason and then call either ``Error::all()``
or ``Error::one()``.  ``Error::all()`` must be called when the failing
code path is executed by *all* MPI processes and the error condition
will appear for *all* MPI processes the same.  If desired, each MPI
process may set a flag to either 0 or 1 and then MPI_Allreduce()
searching for the maximum can be used to determine if there was an error
on *any* of the MPI processes and make this information available to
*all*.  ``Error::one()`` in contrast needs to be called when only one or
a few MPI processes execute the code path or can have the error
condition.  ``Error::all()`` is generally the preferred option.

Calling these functions does not abort LAMMPS directly, but rather
throws either a ``LAMMPSException`` (from ``Error::all()``) or a
``LAMMPSAbortException`` (from ``Error::one()``).  These exceptions are
caught by the LAMMPS ``main()`` program and then handled accordingly.
The reason for this approach is to support applications, especially
graphical applications like :ref:`LAMMPS-GUI <lammps_gui>`, that are
linked to the LAMMPS library and have a mechanism to avoid that an error
in LAMMPS terminates the application. By catching the exceptions, the
application can delete the failing LAMMPS class instance and create a
new one to try again.  In a similar fashion, the :doc:`LAMMPS Python
module <Python_module>` checks for this and then re-throws corresponding
Python exception, which in turn can be caught by the calling Python
code.

There are multiple "signatures" that can be called:

- ``Error::all(FLERR, "Error message")``: this will abort LAMMPS with
  the error message "Error message", followed by the last line of input
  that was read and processed before the error condition happened.

- ``Error::all(FLERR, Error::NOLASTLINE, "Error message")``: this is the
  same as before but without the last line of input.  This is preferred
  for errors that would happen *during* a :doc:`run <run>` or
  :doc:`minimization <minimize>`, since showing the "run" or "minimize"
  command would be the last line, but is unrelated to the error.

- ``Error::all(FLERR, idx, "Error message")``: this is for argument
  parsing where "idx" is the index (starting at 0) of the argument for a
  LAMMPS command that is causing the failure (use -1 for the command
  itself).  For index 0, you need to use the constant ``Error::ARGZERO``
  to work around the inability of some compilers to disambiguate between
  a NULL pointer and an integer constant 0, even with an added type cast.
  The output may also include the last input line *before* and
  *after*, if they differ due to substituting variables.  A textual
  indicator is pointing to the specific word that failed.  Using the
  constant ``Error::NOPOINTER`` in place of the *idx* argument will
  suppress the marker and then the behavior is like the *idx* argument
  is not provided.

FLERR is a macro containing the filename and line where the Error class
is called and that information is appended to the error message.  This
allows to quickly find the relevant source code causing the error.  For
all three signatures, the single string "Error message" may be replaced
with a format string using '{}' placeholders and followed by a variable
number of arguments, one for each placeholder. This format string and
the arguments are then handed for formatting to the `{fmt} library
<https://fmt.dev>`_ (which is bundled with LAMMPS) and thus allow
processing similar to the "format()" functionality in Python.

.. note::

   For commands like :doc:`fix ave/time <fix_ave_time>` that accept
   wildcard arguments, the :cpp:func:`utils::expand_args` function
   may be passed as an optional argument where the function will provide
   a map to the original arguments from the expanded argument indices.

For complex errors, that can have multiple causes and which cannot be
explained in a single line, you can append to the error message, the
string created by :cpp:func:`utils::errorurl`, which then provides a
URL pointing to a paragraph of the :doc:`Errors_details` that
corresponds to the number provided. Example:

.. code-block:: c++

   error->all(FLERR, "Unknown identifier in data file: {}{}", keyword, utils::errorurl(1));

This will output something like this:

.. parsed-literal::

   ERROR: Unknown identifier in data file: Massess
   For more information see https://docs.lammps.org/err0001 (src/read_data.cpp:1482)
   Last input line: read_data       data.peptide

Where the URL points to the first paragraph with explanations on
the :doc:`Errors_details` page in the manual.

**Warnings**

To print warnings, the ``Errors::warning()`` function should be used.
It also requires the FLERR macros as first argument to easily identify
the location of the warning in the source code.  Same as with the error
functions above, the function has two variants: one just taking a single
string as final argument and a second that uses the `{fmt} library
<https://fmt.dev>`_ to make it similar to, say, ``fprintf()``.  One
motivation to use this function is that it will output warnings with
always the same capitalization of the leading "WARNING" string.  A
second is that it has a built in rate limiter.  After a given number (by
default 100), that can be set via the :doc:`thermo_modify command
<thermo_modify>` no more warnings are printed.  Also, warnings are
written consistently to both screen and logfile or not, depending on the
settings for :ref:`screen <screen>` or :doc:`logfile <log>` output.

.. note::

   Unlike ``Error::all()``, the warning function will produce output on
   *every* MPI process, so it typically would be prefixed with an if
   statement testing for ``comm->me == 0``, i.e. limiting output to MPI
   rank 0.

**Informational messages**

Finally, for informational message LAMMPS has the
:cpp:func:`utils::logmesg() convenience function
<LAMMPS_NS::utils::logmesg>`.  It also uses the `{fmt} library
<https://fmt.dev>`_ to support using a format string followed by a
matching number of arguments.  It will output the resulting formatted
text to both, the screen and the logfile and will honor the
corresponding settings about whether this output is active and to which
file it should be send.  Same as for ``Error::warning()``, it would
produce output for every MPI process and thus should usually be called
only on MPI rank 0 to avoid flooding the output when running with many
parallel processes.

Choosing between a custom atom style, fix property/atom, and fix STORE/ATOM
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There are multiple ways to manage per-atom data within LAMMPS.  Often
the per-atom storage is only used locally and managed by the class that
uses it.  If the data has to persist between multiple time steps and
migrate with atoms when they move from sub-domain to sub-domain or
across periodic boundaries, then using a custom atom style, or :doc:`fix
property/atom <fix_property_atom>`, or the internal fix STORE/ATOM are
possible options.

- Using the atom style is usually the most programming effort and mostly
  needed when the per-atom data is an integral part of the model like a
  per-atom charge or diameter and thus should be part of the Atoms
  section of a :doc:`data file <read_data>`.

- Fix property/atom is useful if the data is optional or should be
  entered by the user, or accessed as a (named) custom property. In this
  case the fix should be entered as part of the input (and not
  internally) which allows to enter and store its content with data files.

- Fix STORE/ATOM should be used when the data should be accessed internally
  only and thus the fix can be created internally.

Fix contributions to instantaneous energy, virial, and cumulative energy
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Fixes can calculate contributions to the instantaneous energy and/or
virial of the system, both in a global and peratom sense.  Fixes that
perform thermostatting or barostatting can calculate the cumulative
energy they add to or subtract from the system, which is accessed by
the *ecouple* and *econserve* thermodynamic keywords.  This subsection
explains how both work and what flags to set in a new fix to enable
this functionality.

Let's start with thermostatting and barostatting fixes.  Examples are
the :doc:`fix langevin <fix_langevin>` and :doc:`fix npt <fix_nh>`
commands.  Here is what the fix needs to do:

* Set the variable *ecouple_flag* = 1 in the constructor.  Also set
  *scalar_flag* = 1, *extscalar* = 1, and *global_freq* to a timestep
  increment which matches how often the fix is invoked.
* Implement a compute_scalar() method that returns the cumulative
  energy added or subtracted by the fix, e.g. by rescaling the
  velocity of atoms.  The sign convention is that subtracted energy is
  positive, added energy is negative.  This must be the total energy
  added to the entire system, i.e. an "extensive" quantity, not a
  per-atom energy.  Cumulative means the summed energy since the fix
  was instantiated, even across multiple runs.  This is because the
  energy is used by the *econserve* thermodynamic keyword to check
  that the fix is conserving the total energy of the system,
  i.e. potential energy + kinetic energy + coupling energy = a
  constant.

And here is how the code operates:

* The Modify class makes a list of all fixes that set *ecouple_flag* = 1.
* The :doc:`thermo_style custom <thermo_style>` command defines
  *ecouple* and *econserve* keywords.
* These keywords sum the energy contributions from all the
  *ecouple_flag* = 1 fixes by invoking the *energy_couple()* method in
  the Modify class, which calls the *compute_scalar()* method of each
  fix in the list.

------------------

Next, here is how a fix contributes to the instantaneous energy and
virial of the system.  First, it sets any or all of these flags to a
value of 1 in their constructor:

* *energy_global_flag* to contribute to global energy, example: :doc:`fix indent <fix_indent>`
* *energy_peratom_flag* to contribute to peratom energy, :doc:`fix cmap <fix_cmap>`
* *virial_global_flag* to contribute to global virial, example: :doc:`fix wall <fix_wall>`
* *virial_peratom_flag* to contribute to peratom virial, example: :doc:`fix wall <fix_wall>`

The fix must also do the following:

* For global energy, implement a compute_scalar() method that returns
  the energy added or subtracted on this timestep.  Here the sign
  convention is that added energy is positive, subtracted energy is
  negative.
* For peratom energy, invoke the ev_init(eflag,vflag) function each
  time the fix is invoked, which initializes per-atom energy storage.
  The value of eflag may need to be stored from an earlier call to the
  fix during the same timestep.  See how the :doc:`fix cmap
  <fix_cmap>` command does this in src/MOLECULE/fix_cmap.cpp.  When an
  energy for one or more atoms is calculated, invoke the ev_tally()
  function to tally the contribution to each atom.  Both the ev_init()
  and ev_tally() methods are in the parent Fix class.
* For global and/or peratom virial, invoke the v_init(vflag) function
  each time the fix is invoked, which initializes virial storage.
  When forces on one or more atoms are calculated, invoke the
  v_tally() function to tally the contribution.  Both the v_init() and
  v_tally() methods are in the parent Fix class.  Note that there are
  several variants of v_tally(); choose the one appropriate to your
  fix.

.. note::

   The ev_init() and ev_tally() methods also account for global and
   peratom virial contributions.  Thus you do not need to invoke the
   v_init() and v_tally() methods if the fix also calculates peratom
   energies.

The fix must also specify whether (by default) to include or exclude
these contributions to the global/peratom energy/virial of the system.
For the fix to include the contributions, set either or both of these
variables in the constructor:

* *thermo_energy* = 1, for global and peratom energy
* *thermo_virial* = 1, for global and peratom virial

Note that these variables are zeroed in fix.cpp.  Thus if you don't
set the variables, the contributions will be excluded (by default).

However, the user has ultimate control over whether to include or
exclude the contributions of the fix via the :doc:`fix modify
<fix_modify>` command:

* fix modify *energy yes* to include global and peratom energy contributions
* fix modify *virial yes* to include global and peratom virial contributions

If the fix contributes to any of the global/peratom energy/virial
values for the system, it should be explained on the fix doc page,
along with the default values for the *energy yes/no* and *virial
yes/no* settings of the :doc:`fix modify <fix_modify>` command.

Finally, these 4 contributions are included in the output of 4
computes:

* global energy in :doc:`compute pe <compute_pe>`
* peratom energy in :doc:`compute pe/atom <compute_pe_atom>`
* global virial in :doc:`compute pressure <compute_pressure>`
* peratom virial in :doc:`compute stress/atom <compute_stress_atom>`

These computes invoke a method of the Modify class to include
contributions from fixes that have the corresponding flags set,
e.g. *energy_peratom_flag* and *thermo_energy* for :doc:`compute
pe/atom <compute_pe_atom>`.

Note that each compute has an optional keyword to either include or
exclude all contributions from fixes.  Also note that :doc:`compute pe
<compute_pe>` and :doc:`compute pressure <compute_pressure>` are what
is used (by default) by :doc:`thermodynamic output <thermo_style>` to
calculate values for its *pe* and *press* keywords.

KSpace PPPM FFT grids
^^^^^^^^^^^^^^^^^^^^^

The various :doc:`KSpace PPPM <kspace_style>` styles in LAMMPS use
FFTs to solve Poisson's equation.  This subsection describes:

* how FFT grids are defined
* how they are decomposed across processors
* how they are indexed by each processor
* how particle charge and electric field values are mapped to/from
  the grid

An FFT grid cell is a 3d volume; grid points are corners of a grid
cell and the code stores values assigned to grid points in vectors or
3d arrays.  A global 3d FFT grid has points indexed 0 to N-1 inclusive
in each dimension.

Each processor owns two subsets of the grid, each subset is
brick-shaped.  Depending on how it is used, these subsets are
allocated as a 1d vector or 3d array.  Either way, the ordering of
values within contiguous memory x fastest, then y, z slowest.

For the ``3d decomposition`` of the grid, the global grid is
partitioned into bricks that correspond to the subdomains of the
simulation box that each processor owns.  Often, this is a regular 3d
array (Px by Py by Pz) of bricks, where P = number of processors =
Px * Py * Pz.  More generally it can be a tiled decomposition, where
each processor owns a brick and the union of all the bricks is the
global grid.  Tiled decompositions are produced by load balancing with
the RCB algorithm; see the :doc:`balance rcb <balance>` command.

For the ``FFT decompostion`` of the grid, each processor owns a brick
that spans the entire x dimension of the grid while the y and z
dimensions are partitioned as a regular 2d array (P1 by P2), where P =
P1 * P2.

The following indices store the inclusive bounds of the brick a
processor owns, within the global grid:

.. parsed-literal::

   nFOO_in = 3d decomposition brick
   nFOO_fft = FFT decomposition brick
   nFOO_out = 3d decomposition brick + ghost cells

where ``FOO`` corresponds to ``xlo, xhi, ylo, yhi, zlo,`` or ``zhi``.

The ``in`` and ``fft`` indices are from 0 to N-1 inclusive in each
dimension, where N is the grid size.

The ``out`` indices index an array which stores the ``in`` subset of
the grid plus ghost cells that surround it.  These indices can thus be
< 0 or >= N.

The number of ghost cells a processor owns in each of the 6 directions
is a function of:

.. parsed-literal::

   neighbor skin distance (since atoms can move outside a proc subdomain)
   qdist = offset or charge from atom due to TIP4P fictitious charge
   order = mapping stencil size
   shift = factor used when order is an even number (see below)

Here is an explanation of how the PPPM variables ``order``,
``nlower`` / ``nupper``, ``shift``, and ``OFFSET`` work. They are the
relevant variables that determine how atom charge is mapped to grid
points and how field values are mapped from grid points to atoms:

.. parsed-literal::

   order = # of nearby grid points in each dim that atom charge/field are mapped to/from
   nlower,nupper = extent of stencil around the grid point an atom is assigned to
   OFFSET = large integer added/subtracted when mapping to avoid int(-0.75) = 0 when -1 is the desired result

The particle_map() method assigns each atom to a grid point.

If order is even, say 4:

.. parsed-literal::

   atom is assigned to grid point to its left (in each dim)
   shift = OFFSET
   nlower = -1, nupper = 2, which are offsets from assigned grid point
   window of mapping grid pts is thus 2 grid points to left of atom, 2 to right

If order is odd, say 5:

.. parsed-literal::

   atom is assigned to left/right grid pt it is closest to (in each dim)
   shift = OFFSET + 0.5
   nlower = 2, nupper = 2
   if point is in left half of cell, then window of affected grid pts is 3 grid points to left of atom, 2 to right
   if point is in right half of cell, then window of affected grid pts is 2 grid points to left of atom, 3 to right

These settings apply to each dimension, so that if order = 5, an
atom's charge is mapped to 125 grid points that surround the atom.
