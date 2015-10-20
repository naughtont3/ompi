/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2008 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012      Oak Ridge National Laboratory. All rights reserved.
 * Copyright (c) 2013      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2014-2015 Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"
#include <stdio.h>

#include "ompi/mpi/c/bindings.h"
#include "ompi/runtime/params.h"
#include "ompi/communicator/communicator.h"
#include "ompi/errhandler/errhandler.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/memchecker.h"

#if OMPI_BUILD_MPI_PROFILING
#if OPAL_HAVE_WEAK_SYMBOLS
#pragma weak MPI_Ineighbor_alltoall = PMPI_Ineighbor_alltoall
#endif
#define MPI_Ineighbor_alltoall PMPI_Ineighbor_alltoall
#endif

static const char FUNC_NAME[] = "MPI_Ineighbor_alltoall";


int MPI_Ineighbor_alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                           void *recvbuf, int recvcount, MPI_Datatype recvtype,
                           MPI_Comm comm, MPI_Request *request)
{
    size_t sendtype_size, recvtype_size;
    int err;

    MEMCHECKER(
        memchecker_comm(comm);
        if (MPI_IN_PLACE != sendbuf) {
            memchecker_datatype(sendtype);
            memchecker_call(&opal_memchecker_base_isdefined, (void *)sendbuf, sendcount, sendtype);
        }
        memchecker_datatype(recvtype);
        memchecker_call(&opal_memchecker_base_isaddressable, recvbuf, recvcount, recvtype);
    );

    if (MPI_PARAM_CHECK) {

        /* Unrooted operation -- same checks for all ranks on both
           intracommunicators and intercommunicators */

        err = MPI_SUCCESS;
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (ompi_comm_invalid(comm) || !(OMPI_COMM_IS_CART(comm) || OMPI_COMM_IS_GRAPH(comm) ||
                                         OMPI_COMM_IS_DIST_GRAPH(comm))) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_COMM,
                                          FUNC_NAME);
        } else if (MPI_IN_PLACE == recvbuf) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_ARG,
                                          FUNC_NAME);
        } else if (MPI_IN_PLACE == sendbuf) {
            /* MPI_IN_PLACE is not fully implemented yet,
               return MPI_ERR_INTERN for now */
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_INTERN,
                                          FUNC_NAME);
        } else {
            OMPI_CHECK_DATATYPE_FOR_SEND(err, sendtype, sendcount);
            OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
            OMPI_CHECK_DATATYPE_FOR_RECV(err, recvtype, recvcount);
            OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
        }

        if (MPI_IN_PLACE != sendbuf && !OMPI_COMM_IS_INTER(comm)) {
            ompi_datatype_type_size(sendtype, &sendtype_size);
            ompi_datatype_type_size(recvtype, &recvtype_size);
            if ((sendtype_size*sendcount) != (recvtype_size*recvcount)) {
                return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_TRUNCATE, FUNC_NAME);
            }
        }
    }

    OPAL_CR_ENTER_LIBRARY();

    /* Invoke the coll component to perform the back-end operation */
    err = comm->c_coll.coll_ineighbor_alltoall(sendbuf, sendcount, sendtype,
                                               recvbuf, recvcount, recvtype, comm,
                                               request, comm->c_coll.coll_ineighbor_alltoall_module);
    OMPI_ERRHANDLER_RETURN(err, comm, err, FUNC_NAME);
}
