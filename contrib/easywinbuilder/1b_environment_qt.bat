@call set_vars.bat
@echo About to download Qt installer - you need to install it manually.
@echo  Use default directory "C:\Qt\4.8.5". If it complains about the GCC version just ignore it.
@echo.
@pause
@start %QTDOWNLOADPATH%
@set WAITQT=1
