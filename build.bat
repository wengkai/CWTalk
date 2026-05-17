@echo off

echo Running qmake...

@REM echo Qt version:
@REM qmake --version

@REM echo Cleaning...
@REM rmdir /s /q build 2>nul
@REM mkdir build
cd build

qmake ..\src\cwtalk.pro

echo Building...
mingw32-make

echo Copying data files...
if not exist build\release\data mkdir build\release\data
copy /Y data\cty.dat build\release\data\cty.dat >nul

echo Running...
@REM release\cwtalk.exe

cd ..
pause