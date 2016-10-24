@echo off

set "curdir=%cd%"

REM Put this script under packages directory, bgfx directory should be one level up
REM Make sure you have built the bgfx with release and exe (x64) configurations

mkdir bgfx
mkdir bgfx\bin
mkdir bgfx\include
mkdir bgfx\include\shader
mkdir bgfx\lib

copy ..\bgfx\.build\win64_vs2015\bin\texturecRelease.exe bgfx\bin\texturec.exe
copy ..\bgfx\.build\win64_vs2015\bin\shadercRelease.exe bgfx\bin\shaderc.exe
copy ..\bgfx\.build\win64_vs2015\bin\texturevRelease.exe bgfx\bin\texturev.exe

copy ..\bgfx\.build\win64_vs2015\bin\bgfxRelease.lib bgfx\lib
copy ..\bgfx\.build\win64_vs2015\bin\bgfxDebug.lib bgfx\lib
copy ..\bgfx\.build\win64_vs2015\bin\bgfxDebug.pdb bgfx\lib

copy -r ..\bgfx\include\bgfx bgfx\include
copy ..\bgfx\src\*.sh bgfx\include\shader

REM zip file
echo Set objArgs = WScript.Arguments > _zipIt.vbs
echo InputFolder = objArgs(0) >> _zipIt.vbs
echo ZipFile = objArgs(1) >> _zipIt.vbs
echo CreateObject("Scripting.FileSystemObject").CreateTextFile(ZipFile, True).Write "PK" ^& Chr(5) ^& Chr(6) ^& String(18, vbNullChar) >> _zipIt.vbs
echo Set objShell = CreateObject("Shell.Application") >> _zipIt.vbs
echo Set source = objShell.NameSpace(InputFolder).Items >> _zipIt.vbs
echo objShell.NameSpace(ZipFile).CopyHere(source) >> _zipIt.vbs
echo wScript.Sleep 2000 >> _zipIt.vbs

CScript _zipIt.vbs bgfx bgfx_vc140.zip
del _zipIt.vbs

pause
