@call set_vars.bat
@echo About to download MinGW installer - you need to install it manually. Note:
@echo  Install to default directory: C:\MinGW
@echo  Unselect "... also install support for the graphical user interface."
@echo.
@pause
@start https://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download
@echo.
@echo Once the mingw-get-setup has finished press a key.
@pause
%MINGWPATH%\bin\mingw-get.exe update
%MINGWPATH%\bin\mingw-get.exe install msys-base
%MINGWPATH%\bin\mingw-get install mingw32-make
%MINGWPATH%\bin\mingw-get install msys-wget-bin
%MINGWPATH%\bin\mingw-get install msys-unzip-bin
%MINGWPATH%\bin\mingw-get install msys-perl

%MINGWPATH%\bin\mingw-get.exe install --reinstall --recursive "gcc=4.6.*"
%MINGWPATH%\bin\mingw-get.exe install --reinstall --recursive "gcc-g++=4.6.*"
%MINGWPATH%\bin\mingw-get.exe install --reinstall --recursive "gcc-bin=4.6.*"

%MINGWPATH%\bin\mingw-get install --reinstall --recursive libgmp-dll="5.0.1-*"
%MINGWPATH%\bin\mingw-get install --reinstall --recursive  mingwrt-dev
%MINGWPATH%\bin\mingw-get.exe install --reinstall --recursive "w32api=3.17-2"
%MINGWPATH%\bin\mingw-get.exe install --reinstall --recursive "w32api-dev=3.17-2"

@rem There is a problem with MSYS bash and sh that stalls OpenSSL config on some systems. Using rxvt shell as a workaround.
%MINGWPATH%\bin\mingw-get install msys-rxvt
echo.
