@echo off
echo ==========================================
echo  Compiling Connect 4 in Windows...
echo ==========================================


if not exist obj mkdir obj


gcc -std=c11 -O3 -march=native -flto -Iinclude src/*.c -o connect4.exe

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] La compilacion fallo. Asegurate de tener GCC instalado (MinGW).
    pause
    exit /b %errorlevel%
)

echo.
echo [EXITO] Juego compilado correctamente como 'connect4.exe'
echo.
pause
