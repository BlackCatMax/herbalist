@echo off
setlocal
set UBT=G:\UE_5.7\Engine\Build\BatchFiles\Build.bat
echo Building without cache clearing...
call "%UBT%" ProjectHerbalistEditor Win64 Development "K:\herbalist\ProjectHerbalist\ProjectHerbalist.uproject" -WaitMutex
pause