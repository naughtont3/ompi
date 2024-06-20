/*
 * Tue Nov 29 2022  Thomas Naughton  <naughtont@ornl.gov>
 *
 * Loops over MPI_Alltoall() 'MAX_NLOOP' times.
 *
 * Usage: mpirun -np $nprocs ./a2a_looper  [N]
 *
 *   Optional position-sensitive argument:
 *      arg1 - positive-integer for number of loops
 *
 * If no args are provided the program uses default values.
 *
 * Note: Initial SPC code bits adapted from 'ompi/examples/spc_example.c'
 *
 * TJN: Modified to have only one counter (OMPI_SPC_TIME_ALLTOALL),
 *      also we calculate the diff per-rank at the App level and show
 *      this info each run (at all ranks).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <mpi.h>

int MAX_NLOOP = 100;

int main (int argc, char **argv)
{
    int rank, size;
    int *inbuf = NULL;
    int *outbuf = NULL;
    int i, j;
    int nloop;

    int rc;
    int provided, num, name_len, desc_len, verbosity, bind, var_class, readonly, continuous, atomic, count, index;
    char name[256], description[256];
    MPI_Datatype datatype;
    MPI_T_enum enumtype;
    long long value;
    int found = 0;
    int num_elem = 1024;
    long long _time_alltoall_past_value = 0;

    if (argc > 1) {
        MAX_NLOOP = atoi(argv[1]);
    }

    MPI_Init (&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Counter names to be read by ranks 0 and 1 */
    /* (See also: ompi_spc_counters_t for list) */
    char *counter_name = "runtime_spc_OMPI_SPC_TIME_ALLTOALL";
    MPI_T_pvar_handle handle;
    MPI_T_pvar_session session;

    MPI_T_init_thread(MPI_THREAD_SINGLE, &provided);

    /* Determine the MPI_T pvar indices for the OMPI_BYTES_SENT/RECIEVED_USER SPCs */
    MPI_T_pvar_get_num(&num);

    rc = MPI_T_pvar_session_create(&session);

    for(i = 0; i < num; i++) {
        name_len = desc_len = 256;
        rc = PMPI_T_pvar_get_info(i, name, &name_len, &verbosity,
                                  &var_class, &datatype, &enumtype, description, &desc_len, &bind,
                                  &readonly, &continuous, &atomic);
        if( MPI_SUCCESS != rc )
            continue;

        if(strcmp(name, counter_name) == 0) {
            /* Create the MPI_T sessions/handles for the counters and start the counters */
            rc = MPI_T_pvar_handle_alloc(session, i, NULL, &handle, &count);
            rc = MPI_T_pvar_start(session, handle);
            found = 1;
            //printf("[%d] =====================================\n", rank);
            //printf("[%d] %s -> %s\n", rank, name, description);
            //printf("[%d] =====================================\n", rank);
            //fflush(stdout);
        }
    }

    /* Make sure we found the counters */
    if(found == 0) {
        fprintf(stderr, "ERROR: Couldn't find the appropriate SPC counter in the MPI_T pvars.\n");
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    inbuf  = (int *) malloc ( size * num_elem * sizeof(int) );
    if (NULL == inbuf) {
        fprintf(stderr, "Error: malloc failed (inbuf)\n");
        goto cleanup;
    }

    outbuf  = (int *) malloc ( size * num_elem * sizeof(int) );
    if (NULL == outbuf) {
        fprintf(stderr, "Error: malloc failed (outbuf)\n");
        goto cleanup;
    }

    for (i=0; i < size * num_elem; i++) {
        inbuf[i] = 100 + rank;
        outbuf[i] = 0;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    for (nloop=0; nloop < MAX_NLOOP; nloop++) {
        long long tmp_max;
        int global_rc;
        long long new_value = 0;
        long long diff = 0;

        MPI_Barrier(MPI_COMM_WORLD);
        fflush(NULL);

        rc = MPI_Alltoall(inbuf, num_elem, MPI_INT, outbuf, num_elem, MPI_INT, MPI_COMM_WORLD);

        /* Check if alltoall had any problems? */
        MPI_Allreduce( &rc, &global_rc, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD );
        if (rank == 0) {
            if (global_rc != 0) {
                fprintf(stderr, "Error: Alltoall failed! (rc=%d)\n", global_rc);
                goto cleanup;
            }
        }

        MPI_T_pvar_read(session, handle, &value);
        MPI_Allreduce(&value, &tmp_max, 1, MPI_LONG_LONG, MPI_MAX, MPI_COMM_WORLD);

        rc = ompi_spc_value_diff("OMPI_SPC_TIME_ALLTOALL",
                                 _time_alltoall_past_value,
                                 &new_value,
                                 &diff);


        MPI_Barrier(MPI_COMM_WORLD);

        if ((MAX_NLOOP <= 20) || ( !(nloop % 10) )) {
            //int usecs = 0;
            int usecs = 250000;   /* 0.25 sec */
            //int usecs = 100000;   /* 0.1 sec */
            //int usecs = 2000000;  /* 2 sec */

            printf("%12s: Rank: %5d  Size: %5d  Loop: %8d  %s: %lld  max: %lld prev_value: %lld  new_value: %lld  diff: %lld  -- SLEEP: %dus\n",
                    "a2a_looper", rank, size, nloop, counter_name, value, tmp_max, _time_alltoall_past_value, new_value, diff, usecs);
            usleep(usecs);
        }

        _time_alltoall_past_value = new_value;

        fflush(NULL);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);

#if 0
    printf("[%d] ==========================\n", rank);
    fflush(NULL);

    rc = MPI_T_pvar_read(session, handle, &value);
    printf("TJN: [%d] Value Read: %lld   (%s)\n", rank, value, counter_name);
    fflush(stdout);

    MPI_Barrier(MPI_COMM_WORLD);
#endif

    /* Stop the MPI_T session, free the handle, and then free the session */
    rc = MPI_T_pvar_stop(session, handle);
    rc = MPI_T_pvar_handle_free(session, &handle);

    /* Stop the MPI_T session, free the handle, and then free the session */
    rc = MPI_T_pvar_session_free(&session);

cleanup:
    if (NULL != inbuf)
        free(inbuf);

    if (NULL != outbuf)
        free(outbuf);

    MPI_T_finalize();
    MPI_Finalize();

    return (0);
}
