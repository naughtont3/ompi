# -*- shell-script -*-
#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved
#
# Copyright (c) 2014-2015 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# MCA_orte_rml_ofi_POST_CONFIG(will_build)
# ----------------------------------------
# Only require the tag if we're actually going to be built

# MCA_orte_rml_ofi_CONFIG([action-if-can-compile],
#                    [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_orte_rml_ofi_CONFIG],[
    AC_CONFIG_FILES([orte/mca/rml/ofi/Makefile])

    # ensure we already ran the common libfabric config
    AC_REQUIRE([MCA_opal_common_libfabric_CONFIG])

    OPAL_CHECK_LIBFABRIC([orte_rml_ofi], [orte_rml_ofi_good=1], [orte_rml_ofi_good=0])

    # if check worked, set wrapper flags if so.
    # Evaluate succeed / fail
    AS_IF([test "$orte_rml_ofi_good" = "1"],
          [$1],
          [$2])

    # set build flags to use in makefile
    AC_SUBST([orte_rml_ofi_CPPFLAGS])
    AC_SUBST([orte_rml_ofi_LDFLAGS])
    AC_SUBST([orte_rml_ofi_LIBS])

])dnl
