@echo off
rem  Wrapper batch file that runs the command in thecmd.bat and saves its
rem  exit code to a file so that we can pass it back in the exit code of
rem  doscmd.sh.

call build\thecmd.bat

if errorlevel 1 goto failure
echo 0 result >> build\result.txt
goto end

:failure
echo 1 result >> build\result.txt

:end
exit

