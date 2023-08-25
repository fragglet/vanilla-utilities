#!/bin/sh
#
# Performs OpenWatcom build inside a background dosbox.
# This should be run from the top-level directory.

build/doscmd.sh wmake -e -f makefile.wat -l wmake.err "$@"
result=$?

touch NOTHING.ERR
grep "" $(ls -rt *.ERR)
rm -f NOTHING.ERR

exit $result

