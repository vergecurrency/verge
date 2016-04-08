@call set_vars.bat
@echo Building boost...
@cd %ROOTPATH%\%EWBLIBS%\%BOOST%

@echo bootstrap...
call bootstrap.bat mingw
@echo.
@echo.
@echo building...
b2.exe --build-type=minimal --with-chrono --with-filesystem --with-program_options --with-system --with-thread^
 --layout=versioned -sNO_BZIP2=1 -sNO_ZLIB=1^
 variant=release^
 toolset=gcc^
 link=static^
 threading=multi^
 target-os=windows^
 threadapi=win32^
 cxxflags="%ADDITIONALCCFLAGS%"^
 cflags="%ADDITIONALCCFLAGS%"^
 stage
@cd ..\..\%EWBPATH%
@if not "%RUNALL%"=="1" pause
