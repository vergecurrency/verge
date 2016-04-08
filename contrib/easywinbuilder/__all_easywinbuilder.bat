@echo EasyWinBuilder v0.3
@echo.
@set RUNALL=1
@echo.
@call 1a_environment_mingw.bat
@rem if errorlevel 1 goto error
@call 1b_environment_qt.bat
@rem if errorlevel 1 goto error
@echo.
@call __everything_but_environment_easywinbuilder.bat
@goto end

:error
@echo Fatal error! Errorlevel: %errorlevel%
@pause

:end
