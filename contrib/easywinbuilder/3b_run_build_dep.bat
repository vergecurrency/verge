@call set_vars.bat
@rxvt -e ./build_dep.sh
@if not "%RUNALL%"=="1" pause