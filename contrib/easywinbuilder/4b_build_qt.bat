@call set_vars.bat
@if not "%WAITQT%" == "1" goto continue
@echo Ensure Qt installer has finished.
@pause
:continue

@bash patch_files.sh

@cd %ROOTPATH%
@echo building qt - qmake...

@set QMPS=BOOST_INCLUDE_PATH=%EWBLIBS%/%BOOST%^
 BOOST_LIB_PATH=%EWBLIBS%/%BOOST%/stage/lib^
 BOOST_LIB_SUFFIX=%BOOSTSUFFIX%^
 OPENSSL_INCLUDE_PATH=%EWBLIBS%/%OPENSSL%/include^
 OPENSSL_LIB_PATH=%EWBLIBS%/%OPENSSL%^
 BDB_INCLUDE_PATH=%EWBLIBS%/%BERKELEYDB%/build_unix^
 BDB_LIB_PATH=%EWBLIBS%/%BERKELEYDB%/build_unix^
 MINIUPNPC_INCLUDE_PATH=%EWBLIBS%/%MINIUPNPC%^
 MINIUPNPC_LIB_PATH=%EWBLIBS%/%MINIUPNPC%^
 QMAKE_CXXFLAGS="%ADDITIONALCCFLAGS%"^
 QMAKE_CFLAGS="%ADDITIONALCCFLAGS%"

@%QTPATH%\qmake.exe %QMPS%
@echo.
@echo.
@echo building qt - make...
mingw32-make -f Makefile.Release
@if errorlevel 1 goto continue
@echo !!!!!!! %COINNAME%-qt DONE: Find %COINNAME%-qt.exe in ./release :)
:continue
@echo.
@echo.
@echo.
@echo.
@cd %EWBPATH%
@if not "%RUNALL%"=="1" pause