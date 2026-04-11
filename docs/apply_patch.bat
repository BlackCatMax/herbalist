@echo off

echo === Backup ===
git add .
git commit -m "backup before patch" >nul 2>&1

echo === Checking patch ===
git apply --check patches/fix.patch
if %errorlevel% neq 0 (
    echo ❌ Patch cannot be applied cleanly
    pause
    exit /b
)

echo === Applying patch ===
git apply patches/fix.patch

if %errorlevel% neq 0 (
    echo ❌ Failed to apply patch
    pause
    exit /b
)

echo === Showing diff ===
git diff

echo === Done ===
pause