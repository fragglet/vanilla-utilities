#!/bin/sh
#
# Performs OpenWatcom build inside a background dosbox.
# This should be run from the top-level directory.

build/doscmd.sh make -f makefile.tc all
result=$?

exit $result

