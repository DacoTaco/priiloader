@echo off
<NUL set /p=#define GIT_REV > ../Shared/gitrev.h
for /f "delims=" %%i in ('..\Shared\git.exe describe --always --long') do set version=0x%%i
echo %version% >> ../Shared/gitrev.h
for /f "delims=" %%i in ('..\Shared\git.exe describe --always --long --dirty') do set version_str="%%i"
<NUL set /p=#define GIT_REV_STR >> ../Shared/gitrev.h
echo %version_str% >> ../Shared/gitrev.h
echo generated versions : %version% and %version_str%
