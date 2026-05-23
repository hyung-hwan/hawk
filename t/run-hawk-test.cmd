@echo off
setlocal EnableExtensions EnableDelayedExpansion

if defined HAWK_TEST_COMPILER (
	set "hawk_bin=%HAWK_TEST_COMPILER%"
) else (
	set "hawk_bin=..\bin\hawk.exe"
)

if defined HAWK_LOG_COMPILER set "hawk_bin=%HAWK_LOG_COMPILER%"

for %%I in ("%hawk_bin%") do (
	set "hawk_bin=%%~fI"
	set "hawk_dir=%%~dpI"
)

if not "!hawk_dir:\.libs\=!"=="!hawk_dir!" (
	for %%I in ("!hawk_dir!..\..\lib\.libs") do (
		if exist "%%~fI" set "PATH=%%~fI;!PATH!"
	)
)

if defined HAWK_TEST_OPTS (
	call "!hawk_bin!" %HAWK_TEST_OPTS% %*
) else (
	call "!hawk_bin!" %*
)

exit /b %errorlevel%
