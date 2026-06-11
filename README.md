# APL05 - Chat multiusuario con servidor retransmisor

Proyecto base para el trabajo APL05 de Ingeniería de Protocolos de Comunicaciones.

## Arquitectura

- Servidor:
  - Socket UDP de registro: puerto `40000`
  - Socket UDP de difusión: puerto `40001`
  - Gestiona ambos sockets con `select()`
  - Mantiene una tabla de usuarios registrados
  - Reenvía cada mensaje a todos los usuarios registrados
  - Envía `ACK` al remitente
  - Permite simular pérdida de ACKs para probar reenvíos

- Cliente:
  - Socket UDP de envío: `base_port`
  - Socket UDP de recepción: `base_port + 1`
  - Los dos puertos son consecutivos
  - Se registra en el servidor indicando su puerto de recepción
  - Envía mensajes al puerto de difusión
  - Espera `ACK` con timeout y reintentos
  - Recibe mensajes en un hilo separado
  - Soporta `/list` y `/quit`

## Protocolo

```txt
REG|username|recv_port
REG_OK|username
REG_ERR|reason
MSG|msg_id|username|text
ACK|msg_id
LIST|username
LIST_OK|user1,user2,user3
QUIT|username
QUIT_OK|username
```

## Compilar con MinGW

```bat
build_mingw.bat
```

## Compilar con MSVC

Ejecutar desde "Developer Command Prompt for Visual Studio":

```bat
build_msvc.bat
```

## Ejecutar demo local

Abrir una consola:

```bat
run_server.bat
```

Abrir otras tres consolas:

```bat
run_client_pablo.bat
run_client_ana.bat
run_client_luis.bat
```

También se puede ejecutar manualmente:

```bat
bin\client.exe Pablo 127.0.0.1 40000 40001 50000
bin\client.exe Ana   127.0.0.1 40000 40001 50002
bin\client.exe Luis  127.0.0.1 40000 40001 50004
```

Cada cliente usa dos puertos consecutivos:

```txt
Pablo: 50000 envio, 50001 recepcion
Ana:   50002 envio, 50003 recepcion
Luis:  50004 envio, 50005 recepcion
```

## Probar timeout y reenvío

Ejecutar el servidor con pérdida simulada de ACKs:

```bat
run_server_drop_ack.bat
```

O manualmente:

```bat
bin\server.exe 40000 40001 50
```

El último parámetro indica el porcentaje de ACKs que se descartan artificialmente.

## Comandos del cliente

```txt
/list  muestra usuarios registrados
/quit  sale y elimina el usuario de la tabla del servidor
```

## Próximos pasos posibles

1. Añadir timestamps a los mensajes.
2. Añadir logs a fichero.
3. Mejorar la memoria con capturas de pruebas.
4. Preparar los `.bat` finales para la defensa.

Ya implementado: deduplicación de mensajes retransmitidos por pérdida de ACK y
avisos de entrada/salida de usuarios (mensaje de sistema `MSG|0|Servidor|...`).
