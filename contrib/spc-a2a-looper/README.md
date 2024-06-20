Example SPC Alltoall Looper
---------------------------

Simple example that loops over `MPI_Alltoall()`.  It tests
the `OMPI_SPC_TIME_ALLTOALL` counter.  The test calculates
the diff per-rank at the App level and shows this info each
loop iteration (at all ranks).

The counter acculates and shows the full time for all Alltoall,
but the per-rank view shows the diff per loop.

Pre-reqs
--------
 - Patch with `OMPI_SPC_TIME_ALLTOALL` counter
 - OMPI build with `--enable-spc`

Usage
-----

```sh
 mpirun -np $nprocs ./a2a_looper [N]

   # (optional) arg1 - positive-integer for number of loops
```

Example
-------

Run for just 9 loops:

```sh
 mpirun \
    -np 4 \
    --mca mpi_spc_attach OMPI_SPC_TIME_ALLTOALL \
    --mca mpi_spc_dump_enabled true \
    ./a2a_looper 9
```

Notes
-----
 - Less than 10 will print each loop, and
   above that will print at each interval of 10 loops.

 - Initial SPC code bits adapted from `ompi/examples/spc_example.c`

