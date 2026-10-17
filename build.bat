@echo off
setlocal
cd /d "%~dp0"
echo ========================================================
echo        AION 2 BOT - Compilador Win32 Nativo C Puro      
echo ========================================================

set TCC_BIN=C:\Users\iuryc\tcc\tcc\tcc.exe

if not exist "%TCC_BIN%" (
    echo [ERRO] Compilador TCC nao encontrado em %TCC_BIN%
    pause
    exit /b 1
)

echo [1/3] Compilando main.c com TCC...
"%TCC_BIN%" main.c -o "Aion2_Bot.exe" -lgdi32 -luser32 -lkernel32 -mwindows

if %ERRORLEVEL% equ 0 (
    echo [2/3] Embutindo icone oficial no executavel...
    python -c "import tools_inject_icon; tools_inject_icon.inject_ico(r'Aion2_Bot.exe', r'aion2.ico')"

    echo [3/3] Embutindo manifesto UAC de Administrador...
    python inject_manifest.py "Aion2_Bot.exe"

    echo.
    echo ========================================================
    echo  Aion2_Bot.exe compilado com sucesso no repositorio!
    echo ========================================================
) else (
    echo.
    echo [ERRO] Falha durante a compilacao!
)
