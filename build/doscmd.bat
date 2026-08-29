@echo off
rem  Wrapper batch file that runs thecmd.bat using the dbpipe command,
rem  and exits after it returns.

build\dbpipe build\thecmd.bat

exit
