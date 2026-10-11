@echo off
setlocal
echo ========================================================
echo        AION 2 BOT - Compilador Win32 Nativo C Puro      
echo ========================================================

set TCC_BIN=C:\Users\iuryc\tcc\tcc\tcc.exe
set OUT_EXE=C:\Users\iuryc\Desktop\Aion2_Bot.exe

if not exist "%TCC_BIN%" (
    echo [ERRO] Compilador TCC nao encontrado em %TCC_BIN%
    pause
    exit /b 1
)

echo [1/4] Compilando main.c com TCC...
"%TCC_BIN%" main.c -o "Aion2_Bot.exe" -lgdi32 -luser32 -lkernel32 -mwindows

if %ERRORLEVEL% equ 0 (
    echo [2/4] Embutindo manifesto UAC de Administrador...
    python inject_manifest.py >nul 2>&1

    echo [3/4] Embutindo icone oficial no executavel...
    python -c "import tools_inject_icon; tools_inject_icon.inject_ico(r'Aion2_Bot.exe', r'aion2.ico')" >nul 2>&1

    echo [4/4] Copiando para o Desktop...
    copy /y "Aion2_Bot.exe" "%OUT_EXE%" >nul 2>&1
    copy /y "Aion2_Bot.exe" "C:\Users\iuryc\Desktop\Aion2_Bot_Atualizado.exe" >nul 2>&1
    copy /y "config.ini" "C:\Users\iuryc\Desktop\config.ini" >nul 2>&1
    
    echo.
    echo ========================================================
    echo  Executavel standalone criado com sucesso no Desktop!
    echo ========================================================
) else (
    echo.
    echo [ERRO] Falha durante a compilacao!
)

pause
