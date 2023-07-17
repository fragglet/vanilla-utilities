#!/bin/sh
#
# Fire up a background dosbox and run a command, eg.
#   build/doscmd.sh wmake -f makefile.wat clean
#
# This should be run from the top-level directory.
#
# How this works:
# * a temporary batch file called "thecmd.bat" is generated that gets
#   run inside dosbox by a wrapper batch file named doscmd.bat.
# * the DOS stdout of the command (not stderr) gets written to build.log
#   and then gets passed through to this script's stdout
# * the exit code is written to a temporary file named result.txt and
#   passed back to this script (although only success/failure; not the
#   full exit code)

rm -f build/build.log build/result.txt
touch build/build.log build/result.txt

echo "$@ >> build\\\\build.log" > build/thecmd.bat

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    dosbox -conf build/dosbox.conf -c "build\\doscmd.bat"
cat build/build.log

read result _ < build/result.txt
rm -f build/result.txt build/thecmd.bat

exit $result

