/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2016      Intel, Inc.  All rights reserved.
 * Copyright (c) 2018      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"
#include "coll_tuned.h"

#include <stdio.h>

#include "mpi.h"
#include "ompi/communicator/communicator.h"
#include "ompi/runtime/ompi_spc.h"
#include "ompi/mca/coll/coll.h"
#include "ompi/mca/coll/base/base.h"
#include "ompi/mca/coll/base/coll_base_topo.h"
#include "coll_tuned.h"
#include "coll_tuned_dynamic_rules.h"
#include "coll_tuned_dynamic_file.h"

static int tuned_module_enable(mca_coll_base_module_t *module,
                   struct ompi_communicator_t *comm);
/*
 * Initial query function that is invoked during MPI_INIT, allowing
 * this component to disqualify itself if it doesn't support the
 * required level of thread support.
 */
int ompi_coll_tuned_init_query(bool enable_progress_threads,
                               bool enable_mpi_threads)
{
    return OMPI_SUCCESS;
}


/*
 * Invoked when there's a new communicator that has been created.
 * Look at the communicator and decide which set of functions and
 * priority we want to return.
 */
mca_coll_base_module_t *
ompi_coll_tuned_comm_query(struct ompi_communicator_t *comm, int *priority)
{
    mca_coll_tuned_module_t *tuned_module;

    OPAL_OUTPUT((ompi_coll_tuned_stream, "coll:tuned:module_tuned query called"));

    /**
     * No support for inter-communicator yet.
     */
    if (OMPI_COMM_IS_INTER(comm)) {
        *priority = 0;
        return NULL;
    }

    /**
     * If it is inter-communicator and size is less than 2 we have specialized modules
     * to handle the intra collective communications.
     */
    if (OMPI_COMM_IS_INTRA(comm) && ompi_comm_size(comm) < 2) {
        *priority = 0;
        return NULL;
    }

    tuned_module = OBJ_NEW(mca_coll_tuned_module_t);
    if (NULL == tuned_module) return NULL;

    *priority = ompi_coll_tuned_priority;

    /*
     * Choose whether to use [intra|inter] decision functions
     * and if using fixed OR dynamic rule sets.
     * Right now you cannot mix them, maybe later on it can be changed
     * but this would probably add an extra if and funct call to the path
     */
    tuned_module->super.coll_module_enable = tuned_module_enable;
    tuned_module->super.ft_event = mca_coll_tuned_ft_event;

    /* By default stick with the fixed version of the tuned collectives. Later on,
     * when the module get enabled, set the correct version based on the availability
     * of the dynamic rules.
     */
    tuned_module->super.coll_allgather  = ompi_coll_tuned_allgather_intra_dec_fixed;
    tuned_module->super.coll_allgatherv = ompi_coll_tuned_allgatherv_intra_dec_fixed;
    tuned_module->super.coll_allreduce  = ompi_coll_tuned_allreduce_intra_dec_fixed;
    tuned_module->super.coll_alltoall   = ompi_coll_tuned_alltoall_intra_dec_fixed;
    tuned_module->super.coll_alltoallv  = ompi_coll_tuned_alltoallv_intra_dec_fixed;
    tuned_module->super.coll_alltoallw  = NULL;
    tuned_module->super.coll_barrier    = ompi_coll_tuned_barrier_intra_dec_fixed;
    tuned_module->super.coll_bcast      = ompi_coll_tuned_bcast_intra_dec_fixed;
    tuned_module->super.coll_exscan     = NULL;
    tuned_module->super.coll_gather     = ompi_coll_tuned_gather_intra_dec_fixed;
    tuned_module->super.coll_gatherv    = NULL;
    tuned_module->super.coll_reduce     = ompi_coll_tuned_reduce_intra_dec_fixed;
    tuned_module->super.coll_reduce_scatter = ompi_coll_tuned_reduce_scatter_intra_dec_fixed;
    tuned_module->super.coll_reduce_scatter_block = ompi_coll_tuned_reduce_scatter_block_intra_dec_fixed;
    tuned_module->super.coll_scan       = NULL;
    tuned_module->super.coll_scatter    = ompi_coll_tuned_scatter_intra_dec_fixed;
    tuned_module->super.coll_scatterv   = NULL;

    return &(tuned_module->super);
}

/* We put all routines that handle the MCA user forced algorithm and parameter choices here */
/* recheck the setting of forced, called on module create (i.e. for each new comm) */


/* Congestion - past value for alltoall */
static int _congest_spc_time_alltoall_past_value = 0;



/*
 * Congestion - detection function
 *
 * TJN: This is a very simplistic congestion detection method.
 *      It has several flaws, but shows a basic threshold based
 *      method for determining "congested".
 *      The threshold can be adjust via an MCA parameter.
 *      There are also a few magic envvars to override things
 *      for testing.
 *
 *      NOTE: This is specific to Alltoall for now.
 *
 *      - MCA: coll_tuned_alltoall_congest_threshold
 *          The threshold at which point we decide the difference
 *          in current/past value of SPC indicates "congested".
 *
 *      - EnvVar: 'OMPIX_SKIP_CONGESTED_ALLREDUCE'
 *          This will not detect congestion, becasue only do a
 *          local check and need to have concensus across the comm.
 *          This is just a way to show the difference at each
 *          rank, but always returns "not-congested" overall.
 *
 *      - EnvVar: 'OMPIX_FORCE_CONGESTED'
 *          This is just a hardcoded flag to force the congestion
 *          check to return true regardless of the actual status
 *          of the network.
 */
int
ompi_coll_tuned_isCongested(struct ompi_communicator_t *comm)
{
    long long new_value = 0;
    long long diff = 0;
    int rc;
    int comm_rank;
    long long diff_max = 0;

    OPAL_OUTPUT((ompi_coll_tuned_stream, "coll:tuned: alltoall_congest_threshold = %d\n", ompi_coll_tuned_alltoall_congest_threshold));

    /* Get our local congestion info */
    rc = ompi_spc_value_diff("OMPI_SPC_TIME_ALLTOALL",
                    _congest_spc_time_alltoall_past_value,
                    &new_value,
                    &diff);
    if (0 != rc) {
        return 0; /* Ignore error for now (treat as not congested) */
    }

    comm_rank = ompi_comm_rank(comm);

    OPAL_OUTPUT((ompi_coll_tuned_stream, " #-- DBG: (Rank %d) MY-CONGESTION-INFO (thresh=%d, past_value=%d, new_value=%d, diff=%d)\n", comm_rank, ompi_coll_tuned_alltoall_congest_threshold, _congest_spc_time_alltoall_past_value, new_value, diff));
    /* TJN: quick hack to see congest diff info per rank (w/o full verbose) */
    if (NULL != getenv("OMPIX_SHOW_CONGEST_INFO")) {
        fprintf(stderr, " #-- DBG: (Rank %d) MY-CONGESTION-INFO (thresh=%d, past_value=%d, new_value=%d, diff=%d)\n", comm_rank, ompi_coll_tuned_alltoall_congest_threshold, _congest_spc_time_alltoall_past_value, new_value, diff);
    }

    _congest_spc_time_alltoall_past_value = new_value;

    /*
     * TJN: Skip the allreduce and do *only* the local diff check,
     *      but in this case we will not adjust the algorithm, just
     *      report the difference and move on.
     */
    if (NULL != getenv("OMPIX_SKIP_CONGESTED_ALLREDUCE")) {

        /* diff: (local-only) Decide how different from past */
        if ((0  != diff) && (diff  > ompi_coll_tuned_alltoall_congest_threshold)) {
            OPAL_OUTPUT((ompi_coll_tuned_stream, " #-- DBG: (Rank %d) LOCAL-ONLY CONGESTION SKIP-ALLREDUCE (thresh=%d, new_value=%d, diff=%d)!\n", comm_rank, ompi_coll_tuned_alltoall_congest_threshold, new_value, diff));
            fprintf(stderr, " #-- DBG: (Rank %d) LOCAL-ONLY CONGESTION SKIP-ALLREDUCE (thresh=%d, new_value=%d, diff=%d)!\n", comm_rank, ompi_coll_tuned_alltoall_congest_threshold, new_value, diff);

            return 0;  /* Always return 'not congested' for this case */
        }

    } else {
        comm_rank = ompi_comm_rank(comm);

        /*
         * Aggregate all of the information using MPI_Allreduce(MAX)
         * on diff value to see if any rank in comm exceeded the
         * max threshold.
         *
         * TODO TJN: Change this when we add congestion checks for MPI_Reduce()!
         */
        (void)comm->c_coll->coll_allreduce(&diff, &diff_max,
                                        1, MPI_LONG_LONG, MPI_MAX,
                                        comm,
                                        comm->c_coll->coll_allreduce_module);

        (void)comm->c_coll->coll_barrier(comm, comm->c_coll->coll_barrier_module);

        /* diff_max: (global max) Decide how different from past */
        if ((0  != diff_max) && (diff_max  > ompi_coll_tuned_alltoall_congest_threshold)) {
            OPAL_OUTPUT((ompi_coll_tuned_stream, " #-- DBG: (Rank %d) EXCEED CONGESTION THRESHOLD -- CONGESTED (thresh=%d, new_value=%d, diff=%d, diff_max=%d)!\n", comm_rank, ompi_coll_tuned_alltoall_congest_threshold, new_value, diff, diff_max));
            //fprintf(stderr, " #-- DBG: (Rank %d) EXCEED CONGESTION THRESHOLD -- CONGESTED (thresh=%d, new_value=%d, diff=%d, diff_max=%d)!\n", comm_rank, ompi_coll_tuned_alltoall_congest_threshold, new_value, diff, diff_max);
            return 1;  /* Yes congested */
        }
    }

#if 1
    /* XXX: for now if have env var set we call it congested */
    if (NULL != getenv("OMPIX_FORCE_CONGESTED")) {
        fprintf(stderr, " #-- DBG: TJN_HACK_CONGESTED CONGESTION FORCED -- CONGESTED!\n");
        return 1;  /* Yes congested */
    }
#endif

    return 0; /* Not congested */
}

/*
 * Congestion - get algorithm to use when congested
 *
 * TJN: This is a very simplistic method to return the registered
 *      default algorithm to use when congestion is detected.
 *      This is set via an MCA parameter, which can be overriden
 *      at runtime.
 *
 *      NOTE: This is specific to Alltoall for now.
 *
 *      - MCA: coll_tuned_alltoall_congest_algorithm
 *          The alltoall algorithm to use when we detect
 *          congestion, i.e., ompi_coll_tuned_isCongested() is true.
 */
int
ompi_coll_tuned_get_congest_algo(void)
{
    int alg = -1;

    /* TODO: Should check this is a valid alltoall algo */
    alg = ompi_coll_tuned_alltoall_congest_algorithm;

    OPAL_OUTPUT((ompi_coll_tuned_stream, "coll:tuned: alltoall_congest_algorithm = %d\n", alg));

    return (alg);
}


static int
ompi_coll_tuned_forced_getvalues( enum COLLTYPE type,
                                  coll_tuned_force_algorithm_params_t *forced_values )
{
    coll_tuned_force_algorithm_mca_param_indices_t* mca_params;
    const int *tmp = NULL;

    mca_params = &(ompi_coll_tuned_forced_params[type]);

    /**
     * Set the selected algorithm to 0 by default. Later on we can check this against 0
     * to see if it was setted explicitly (if we suppose that setting it to 0 enable the
     * default behavior) or not.
     */
    mca_base_var_get_value(mca_params->algorithm_param_index, &tmp, NULL, NULL);
    forced_values->algorithm = tmp ? tmp[0] : 0;

#if 0
    /* Congestion stuff (likely cut this) */
    /* TJN: We are only changing the algorithm for ALLTOALL (if congested) */
    if( ALLTOALL == type ) {

        //fprintf(stderr, " #-- DBG: FORCED_GETVALUES (cur) algorithm = %d\n",
        //  forced_values->algorithm);

        if ( ompi_coll_tuned_isCongested() ) {
            int new_alg;
            new_alg = ompi_coll_tuned_get_congest_algo();
            if (new_alg >= 0) {
                forced_values->algorithm = new_alg;

                //fprintf(stderr, " #-- DBG: HACK OVERRIDE ALLTOALL FORCED_GETVALUES algorithm = %d (new_alg=%d)\n",
                //      forced_values->algorithm, new_alg);
            }
        }

        //fprintf(stderr, " #-- DBG: FORCED_GETVALUES (new) algorithm = %d\n",
        //  forced_values->algorithm);
    }
#endif

    if( BARRIER != type ) {
        mca_base_var_get_value(mca_params->segsize_param_index, &tmp, NULL, NULL);
        if (tmp) forced_values->segsize = tmp[0];
        mca_base_var_get_value(mca_params->tree_fanout_param_index, &tmp, NULL, NULL);
        if (tmp) forced_values->tree_fanout = tmp[0];
        mca_base_var_get_value(mca_params->chain_fanout_param_index, &tmp, NULL, NULL);
        if (tmp) forced_values->chain_fanout = tmp[0];
        mca_base_var_get_value(mca_params->max_requests_param_index, &tmp, NULL, NULL);
        if (tmp) forced_values->max_requests = tmp[0];
    }
    return (MPI_SUCCESS);
}

#define COLL_TUNED_EXECUTE_IF_DYNAMIC(TMOD, TYPE, EXECUTE)              \
    {                                                                   \
        int need_dynamic_decision = 0;                                  \
        ompi_coll_tuned_forced_getvalues( (TYPE), &((TMOD)->user_forced[(TYPE)]) ); \
        (TMOD)->com_rules[(TYPE)] = NULL;                               \
        if( 0 != (TMOD)->user_forced[(TYPE)].algorithm ) {              \
            need_dynamic_decision = 1;                                  \
        }                                                               \
        if( NULL != mca_coll_tuned_component.all_base_rules ) {         \
            (TMOD)->com_rules[(TYPE)]                                   \
                = ompi_coll_tuned_get_com_rule_ptr( mca_coll_tuned_component.all_base_rules, \
                                                    (TYPE), size );     \
            if( NULL != (TMOD)->com_rules[(TYPE)] ) {                   \
                need_dynamic_decision = 1;                              \
            }                                                           \
        }                                                               \
        if( 1 == need_dynamic_decision ) {                              \
            OPAL_OUTPUT((ompi_coll_tuned_stream,"coll:tuned: enable dynamic selection for "#TYPE)); \
            EXECUTE;                                                    \
        }                                                               \
    }

/*
 * Init module on the communicator
 */
static int
tuned_module_enable( mca_coll_base_module_t *module,
                     struct ompi_communicator_t *comm )
{
    int size;
    mca_coll_tuned_module_t *tuned_module = (mca_coll_tuned_module_t *) module;
    mca_coll_base_comm_t *data = NULL;

    OPAL_OUTPUT((ompi_coll_tuned_stream,"coll:tuned:module_init called."));

    /* Allocate the data that hangs off the communicator */
    if (OMPI_COMM_IS_INTER(comm)) {
        size = ompi_comm_remote_size(comm);
    } else {
        size = ompi_comm_size(comm);
    }

    /**
     * we still malloc data as it is used by the TUNED modules
     * if we don't allocate it and fall back to a BASIC module routine then confuses debuggers
     * we place any special info after the default data
     *
     * BUT on very large systems we might not be able to allocate all this memory so
     * we do check a MCA parameter to see if if we should allocate this memory
     *
     * The default is set very high
     */

    /* prepare the placeholder for the array of request* */
    data = OBJ_NEW(mca_coll_base_comm_t);
    if (NULL == data) {
        return OMPI_ERROR;
    }

    if (ompi_coll_tuned_use_dynamic_rules) {
        OPAL_OUTPUT((ompi_coll_tuned_stream,"coll:tuned:module_init MCW & Dynamic"));

        /**
         * next dynamic state, recheck all forced rules as well
         * warning, we should check to make sure this is really an INTRA comm here...
         */
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, ALLGATHER,
                                      tuned_module->super.coll_allgather  = ompi_coll_tuned_allgather_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, ALLGATHERV,
                                      tuned_module->super.coll_allgatherv = ompi_coll_tuned_allgatherv_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, ALLREDUCE,
                                      tuned_module->super.coll_allreduce  = ompi_coll_tuned_allreduce_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, ALLTOALL,
                                      tuned_module->super.coll_alltoall   = ompi_coll_tuned_alltoall_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, ALLTOALLV,
                                      tuned_module->super.coll_alltoallv  = ompi_coll_tuned_alltoallv_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, ALLTOALLW,
                                      tuned_module->super.coll_alltoallw  = NULL);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, BARRIER,
                                      tuned_module->super.coll_barrier    = ompi_coll_tuned_barrier_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, BCAST,
                                      tuned_module->super.coll_bcast      = ompi_coll_tuned_bcast_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, EXSCAN,
                                      tuned_module->super.coll_exscan     = ompi_coll_tuned_exscan_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, GATHER,
                                      tuned_module->super.coll_gather     = ompi_coll_tuned_gather_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, GATHERV,
                                      tuned_module->super.coll_gatherv    = NULL);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, REDUCE,
                                      tuned_module->super.coll_reduce     = ompi_coll_tuned_reduce_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, REDUCESCATTER,
                                      tuned_module->super.coll_reduce_scatter = ompi_coll_tuned_reduce_scatter_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, REDUCESCATTERBLOCK,
                                      tuned_module->super.coll_reduce_scatter_block = ompi_coll_tuned_reduce_scatter_block_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, SCAN,
                                      tuned_module->super.coll_scan       = ompi_coll_tuned_scan_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, SCATTER,
                                      tuned_module->super.coll_scatter    = ompi_coll_tuned_scatter_intra_dec_dynamic);
        COLL_TUNED_EXECUTE_IF_DYNAMIC(tuned_module, SCATTERV,
                                      tuned_module->super.coll_scatterv   = NULL);
    }

    /* general n fan out tree */
    data->cached_ntree = NULL;
    /* binary tree */
    data->cached_bintree = NULL;
    /* binomial tree */
    data->cached_bmtree = NULL;
    /* binomial tree */
    data->cached_in_order_bmtree = NULL;
    /* k-nomial tree */
    data->cached_kmtree = NULL;
    /* chains (fanout followed by pipelines) */
    data->cached_chain = NULL;
    /* standard pipeline */
    data->cached_pipeline = NULL;
    /* in-order binary tree */
    data->cached_in_order_bintree = NULL;

    /* All done */
    tuned_module->super.base_data = data;

    OPAL_OUTPUT((ompi_coll_tuned_stream,"coll:tuned:module_init Tuned is in use"));
    return OMPI_SUCCESS;
}

int mca_coll_tuned_ft_event(int state) {
    if(OPAL_CRS_CHECKPOINT == state) {
        ;
    }
    else if(OPAL_CRS_CONTINUE == state) {
        ;
    }
    else if(OPAL_CRS_RESTART == state) {
        ;
    }
    else if(OPAL_CRS_TERM == state ) {
        ;
    }
    else {
        ;
    }

    return OMPI_SUCCESS;
}
