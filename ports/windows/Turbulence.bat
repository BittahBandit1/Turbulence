@echo off
rem Turbulence launcher. Put your extracted Sonic Riders (USA) folder (it
rem contains sys\main.dol) in "game" next to this file, or pass --game.
setlocal
set "ROOT=%~dp0"
if not exist "%ROOT%user\Config" mkdir "%ROOT%user\Config"
if not exist "%ROOT%user\Config\GCPadNew.ini" copy "%ROOT%GCPadNew.ini" "%ROOT%user\Config\" >nul
if not exist "%ROOT%game\sys\main.dol" (
  echo Turbulence: copy your extracted Sonic Riders USA folder to "%ROOT%game" ^(it must contain sys\main.dol^).
  echo From an ISO: py extract-sonic-riders.py "Sonic Riders.iso" game
  echo or in Dolphin: Properties, Filesystem, right-click the disc, Extract Entire Disc.
  pause
  exit /b 1
)
"%ROOT%moderngekko-run.exe" --game "%ROOT%game" --module "%ROOT%module\gGXEE8P_recomp.dll" --user-dir "%ROOT%user" %*
