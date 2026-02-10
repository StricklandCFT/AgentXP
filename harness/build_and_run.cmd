@echo off
REM Build and run the Procmon harness on XP (VS2008 + Win7.1 SDK)
REM Adjust VSINSTALLDIR if needed.

set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio 9.0
call "%VSINSTALLDIR%\VC\vcvarsall.bat" x86

cl /EHsc /O2 /MT /DWIN32 /D_WINDOWS procmon_harness.cpp /link /SUBSYSTEM:CONSOLE,5.01

REM Example run (device name + dump path)
procmon_harness.exe --device \\.\ProcmonDebugLogger --dump procmon-raw.bin
