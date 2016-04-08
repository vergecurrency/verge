@call set_vars.bat
@if not "%WAITMINGW%" == "1" goto continue
@echo Ensure MinGW installer has finished.
@pause
:continue
@bash download.sh
@if not "%RUNALL%"=="1" pause
