@echo off
setlocal

:: Путь к корню проекта
set PROJECT_DIR=K:\herbalist\ProjectHerbalist
set UPROJECT=%PROJECT_DIR%\ProjectHerbalist.uproject

:: Удаляем папки кэша, если они существуют
if exist "%PROJECT_DIR%\Intermediate" (
    echo Deleting Intermediate...
    rmdir /s /q "%PROJECT_DIR%\Intermediate"
)

if exist "%PROJECT_DIR%\Binaries" (
    echo Deleting Binaries...
    rmdir /s /q "%PROJECT_DIR%\Binaries"
)

if exist "%PROJECT_DIR%\DerivedDataCache" (
    echo Deleting DerivedDataCache...
    rmdir /s /q "%PROJECT_DIR%\DerivedDataCache"
)

:: Перегенерация файлов проекта (sln, vcxproj и т.д.)
echo Generating project files...
call "G:\UE_5.7\Engine\Build\BatchFiles\GenerateProjectFiles.bat" "%UPROJECT%"

:: Путь к UnrealBuildTool
set UBT=G:\UE_5.7\Engine\Build\BatchFiles\Build.bat

:: Запуск сборки (Development Editor)
echo Starting build...
call "%UBT%" ProjectHerbalistEditor Win64 Development "%UPROJECT%" -WaitMutex

pause