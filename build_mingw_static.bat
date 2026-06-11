@echo off
setlocal

if not exist bin mkdir bin

echo Compilando servidor (estatico)...
g++ -std=c++17 -Wall -Wextra -Icommon common\protocol.cpp server\server.cpp -o bin\server.exe -lws2_32 -static
if errorlevel 1 goto error

echo Compilando cliente (estatico)...
g++ -std=c++17 -Wall -Wextra -Icommon common\protocol.cpp client\client.cpp -o bin\client.exe -lws2_32 -static
if errorlevel 1 goto error

echo.
echo Compilacion estatica completada correctamente.
echo Los .exe de bin\ ya no dependen de las DLLs de MinGW.
echo Puedes copiar bin\client.exe a otro portatil y funcionara solo.
goto end

:error
echo.
echo Hubo errores de compilacion.

:end
endlocal
