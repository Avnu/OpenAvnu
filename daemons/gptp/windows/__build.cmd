@ECHO OFF

::
:: Batch for compiling solution via command line using MSBuild
::
::   Author: Michael Welter <michael@cetoncorp.com>
:: 
:: /x64     = 64-bit
:: /Win32   = 32-bit
::
:: /Release = Release build
:: /Debug   = Debug build
::
::   Defaults to /Release /Win32 /x64
::

SETLOCAL ENABLEDELAYEDEXPANSION

SET MSBuild=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\MSBuild.exe
IF NOT EXIST "%MSBuild%" (
	ECHO Error: MSBuild not found - "%MSBuild%"
	GOTO Exit
)

IF "%1"=="/?" (GOTO Help)
IF "%1"=="-?" (GOTO Help)

FOR %%* IN (%*) DO (
	SET arg=%%*
	IF /I "%%*"=="/x64" (SET Platform=!Platform! !arg:~1!)
	IF /I "%%*"=="/Win32" (SET Platform=!Platform! !arg:~1!)
	IF /I "%%*"=="/Release" (SET Configuration=!Configuration! !arg:~1!)
	IF /I "%%*"=="/Debug" (SET Configuration=!Configuration! !arg:~1!)
	IF /I "%%*"=="help" (GOTO Help)
)

IF "%Platform%"=="" (SET Platform=Win32 x64)
IF "%Configuration%"=="" (SET Configuration=Release)

FOR %%* IN ("%~dp0*.sln") DO (SET Filename="%%*")
IF NOT EXIST "%Filename%" (
	ECHO Error: Solution "%Filename%" not found
	GOTO Exit
)

FOR %%C IN (%Configuration%) DO FOR %%P IN (%Platform%) DO (
	ECHO %%C^|%%P
	%MSBuild% /maxcpucount /nologo "%Filename%" /p:Configuration="%%C" /p:Platform="%%P" /t:Clean;Build
)

:Exit

ENDLOCAL

EXIT /B

:Help

ECHO.
ECHO. Batch for compiling solution via command line using MSBuild
ECHO.
ECHO.   Author: Michael Welter ^<michael@cetoncorp.com^>
ECHO. 
ECHO. Usage: %~nx0 [/x64][/Win32][/Release][/Debug]
ECHO. 
ECHO. /x64     = 64-bit
ECHO. /Win32   = 32-bit
ECHO.
ECHO. /Release = Release build
ECHO. /Debug   = Debug build
ECHO.
ECHO.   Defaults to /Release /Win32 /x64
ECHO.

GOTO Exit