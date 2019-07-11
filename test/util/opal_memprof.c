/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2019      UT-Battelle, LLC. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include "opal/runtime/opal.h"
#include "support.h"

static bool test1(void);
static bool test2(void);


int main(int argc, char* argv[])
{
    opal_init(&argc, &argv);

    test_init("opal_memhook");


    if (test1()) {
        test_success();
    }
    else {
      test_failure("opal_memhook test1 failed");
    }

    if (test2()) {
        test_success();
    }
    else {
      test_failure("opal_memhook test2 failed");
    }

    opal_finalize();

    test_finalize();
    return 0;
}


/*
 * XXX: Test following
 *
 *   OPAL_MEMPROF_TRACK_ALLOC(name, size)
 *   OPAL_MEMPROF_TRACK_DEALLOC(name, size)
 */
static bool test1(void)
{
    int i, *data = NULL, data_size, base_val;

    base_val = 100;
    data_size = 10 * sizeof(int);

    /*
     * XXX: Not sure this is working correctly to capture
     *      what we need for the memprof hooks.
     *      Test this with 'tau_exec'!!!
     */
    data = (int*) malloc( data_size );
    OPAL_MEMPROF_TRACK_ALLOC("test1_data", data_size);

    for (i=0; i < 10; i++) {
        data[i] = base_val + i;
    }

    for (i=0; i < 10; i++) {
        printf("%s: data[%d] = %d  (should be: %d + %d)\n",
               __func__, i, data[i], base_val, i);
    }

    if (NULL != data) {
        free(data);
        OPAL_MEMPROF_TRACK_DEALLOC("test1_data", data_size );
    }

    return true;
}


/*
 * XXX: Test following
 *
 * OPAL_MEMPROF_START_ALLOC(name, size, include_in_parent)
 * OPAL_MEMPROF_STOP_ALLOC(name, record)
 * OPAL_MEMPROF_START_DEALLOC(name, size, include_in_parent)
 * OPAL_MEMPROF_STOP_DEALLOC(name, record)
 */
static bool test2(void)
{
    printf("TODO: write test2\n");
    return true;
}
