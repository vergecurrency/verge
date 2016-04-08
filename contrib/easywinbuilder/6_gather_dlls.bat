@call set_vars.bat
copy %QTPATH%\QtCore4.dll %ROOTPATH%\release\
copy %QTPATH%\QtGui4.dll %ROOTPATH%\release\
copy %QTPATH%\QtNetwork4.dll %ROOTPATH%\release\
copy C:\MinGW\bin\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy "C:\MinGW\bin\libstdc++-6.dll" %ROOTPATH%\release\
copy C:\MinGW\bin\mingwm10.dll %ROOTPATH%\release\

copy C:\MinGW\bin\libgcc_s_dw2-1.dll %ROOTPATH%\src\
copy "C:\MinGW\bin\libstdc++-6.dll" %ROOTPATH%\src\
@if not "%RUNALL%"=="1" pause