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


static bool test1(void)
{
    printf("DBG: Hello1\n");
    return true;
}

static bool test2(void)
{
    printf("DBG: Hello2 BAD\n");
    return false;
}

