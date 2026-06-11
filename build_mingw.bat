@echo off
setlocal

if not exist bin mkdir bin

echo Compilando servidor...
g++ -std=c++17 -Wall -Wextra -Icommon common\protocol.cpp server\server.cpp -o bin\server.exe -lws2_32
if errorlevel 1 goto error

echo Compilando cliente...
g++ -std=c++17 -Wall -Wextra -Icommon common\protocol.cpp client\client.cpp -o bin\client.exe -lws2_32
if errorlevel 1 goto error

echo.
echo Compilacion completada correctamente.
echo Ejecuta run_server.bat y luego varios run_client_*.bat
goto end

:error
echo.
echo Hubo errores de compilacion.

:end
endlocal
