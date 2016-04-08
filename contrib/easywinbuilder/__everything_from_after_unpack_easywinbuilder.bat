@set RUNALL=1
@call 3a_build_boost.bat
@if errorlevel 1 goto error
@call 3b_run_build_dep.bat
@if errorlevel 1 goto error
@call 3c_build_miniupnpc.bat
@if errorlevel 1 goto error
@call 4a_build_daemon.bat
@if errorlevel 1 goto error
@call 4b_build_qt.bat
@if errorlevel 1 goto error
@rem call 5a_run_hash_daemon.bat
@if errorlevel 1 goto error
@rem @call 5b_run_hash_qt.bat
@if errorlevel 1 goto error
@call 6_gather_dlls.bat
@if errorlevel 1 goto error
@echo.
@echo.
@goto end

:error
@echo Fatal error! Errorlevel: %errorlevel%

:end
@pause


