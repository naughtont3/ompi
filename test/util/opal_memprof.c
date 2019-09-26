/*
 * Copyright (c) 2019      UT-Battelle, LLC. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * Description: This is a trivial test to exercise
 *   the memory profile functions added to OPAL.
 *   Note, we have the program call MPI_Init() so
 *   we can easily capture memory counters, e.g., via
 *   Tau 'mpi' module).
 *
 * Usage:
 *     mpicc opal_memprof.c -o opal_memprof
 *     mpirun -np 2 tau_exec -T mpi,pdt,pthread ./opal_memprof
 *     grep -e test1 -e test2 profile.*.0.0
 *
 *   When done, we will have the typical counters
 *   as well as the item add in this test:
 *          'alloc test1_data'
 *          'free  test1_data'
 *          'alloc test2_data'
 *          'free  test2_data'
 *
 *   We do not actually allocate memory because that
 *   is not needed to test the event tracking calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mpi.h"

#include "opal/runtime/opal.h"
#include "opal/util/memprof.h"

#define SIZE  40

#define RET_PASS  1
#define RET_FAIL  0

static int test1(int rank);
static int test2(int rank);

int main(int argc, char* argv[])
{
    int rank;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (test1(rank)) {
        printf("(rank %d) test1 SUCCESS\n", rank);
    }
    else {
        printf("(rank %d) test1 FAILED\n", rank);
    }

    if (test2(rank)) {
        printf("(rank %d) test2 SUCCESS\n", rank);
    }
    else {
        printf("(rank %d) test2 FAILED\n", rank);
    }

    MPI_Finalize();

    return 0;
}

/*
 * Test following:
 *
 *   OPAL_MEMPROF_TRACK_ALLOC(name, size)
 *   OPAL_MEMPROF_TRACK_DEALLOC(name, size)
 */
static int test1(int rank)
{
    OPAL_MEMPROF_TRACK_ALLOC("test1_data", SIZE);

    printf("(rank %d) %s: allocated size = %d\n",
           rank, __func__, SIZE);

    /* Add slight delay in case we test tracing. */
    usleep(20);

    OPAL_MEMPROF_TRACK_DEALLOC("test1_data", SIZE);

    return RET_PASS;
}


/*
 * Test following:
 *
 *   OPAL_MEMPROF_START_ALLOC(name, size, include_in_parent)
 *   OPAL_MEMPROF_STOP_ALLOC(name, record)
 *   OPAL_MEMPROF_START_DEALLOC(name, size, include_in_parent)
 *   OPAL_MEMPROF_STOP_DEALLOC(name, record)
 */
static int test2(int rank)
{
    int i;

    OPAL_MEMPROF_START_ALLOC("test2_data", SIZE, 0);

    printf("(rank %d) %s: allocate size = %d\n",
           rank, __func__, SIZE);

    /* Add slight delay in case we test tracing. */
    usleep(20);

    OPAL_MEMPROF_STOP_ALLOC("test2_data", 1);

    /* Add slight delay in case we test tracing. */
    usleep(20);

    /*
     * NOTE: Hierarchical deallocation routines are not
     *       currently supported by Tau.  So always
     *       just use OPAL_MEMPROF_TRACK_DEALLOC.
     */
    OPAL_MEMPROF_TRACK_DEALLOC("test2_data", SIZE);

    return RET_PASS;
}
