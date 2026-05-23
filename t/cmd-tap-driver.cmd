REM set HAWK_TEST_COMPILER=Z:\path\to\hawk\bld-mingw64\bin\.libs\hawk.exe
REM Z:\path\to\hawk\t\cmd-tap-driver.cmd --test-name h-011 --log-file h-011.log --trs-file h-011.trs -- Z:\path\to\hawk\t\t\run-hawk-test.cmd -f ..\..\t\h-011.hawk
REM

@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "test_name="
set "log_file="
set "trs_file="

:parse
if "%~1"=="" goto usage
if "%~1"=="--" goto seen_separator
if "%~1"=="--test-name" goto set_test_name
if "%~1"=="--log-file" goto set_log_file
if "%~1"=="--trs-file" goto set_trs_file

echo ERROR: unknown option %~1 1>&2
goto usage_exit

:set_test_name
if "%~2"=="" goto usage
set "test_name=%~2"
shift
shift
goto parse

:set_log_file
if "%~2"=="" goto usage
set "log_file=%~2"
shift
shift
goto parse

:set_trs_file
if "%~2"=="" goto usage
set "trs_file=%~2"
shift
shift
goto parse

:seen_separator
shift

:run
if "%~1"=="" goto usage
if not defined test_name set "test_name=cmd-test"
if not defined log_file set "log_file=%test_name%.log"
if not defined trs_file set "trs_file=%test_name%.trs"

call %1 %2 %3 %4 %5 %6 %7 %8 %9 >"%log_file%" 2>&1
set "cmd_status=%errorlevel%"

type "%log_file%"

set "result=PASS"
if not "%cmd_status%"=="0" set "result=FAIL"

findstr /b /c:"not ok " "%log_file%" >nul 2>&1
if not errorlevel 1 set "result=FAIL"

findstr /b /c:"1..0 # SKIP" "%log_file%" >nul 2>&1
if not errorlevel 1 set "result=SKIP"

(
	echo :test-result: %result%
	echo :global-test-result: %result%
	echo :recheck: no
	echo :copy-in-global-log: no
) >"%trs_file%"

echo %result%: %test_name%

if /i "%result%"=="PASS" exit /b 0
if /i "%result%"=="SKIP" exit /b 0
exit /b 1

:usage
echo Usage: %~nx0 --test-name NAME [--log-file FILE] [--trs-file FILE] -- COMMAND [ARGS...] 1>&2

:usage_exit
exit /b 2
