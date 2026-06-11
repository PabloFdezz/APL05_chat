# MEMORIA – APL05: Servicio Conversacional Multiusuario

Pablo Fernández Hernando
Pablo Agustín Myrick Ruiz

---

## 1. Descripción técnica de la aplicación

La aplicación implementa un servicio de chat multiusuario basado en una **entidad
central retransmisora** que difunde, a todos los usuarios registrados, los mensajes
que recibe de cualquiera de ellos. Toda la comunicación se realiza sobre **UDP**.

### 1.1 Entidades implicadas

- **Entidad retransmisora (servidor)** — Proceso único que ofrece dos servicios sobre
  dos puertos UDP distintos:
  - *Puerto de registro* (por defecto `40000`): da de alta a los usuarios.
  - *Puerto de difusión de datos* (por defecto `40001`): recibe mensajes, los
    retransmite a todos los registrados y devuelve un asentimiento (ACK) al emisor.
  Mantiene en memoria una **tabla de usuarios** con el nombre y la dirección
  (IP + puerto de recepción) de cada cliente.

- **Usuario (cliente)** — Proceso que abre **dos sockets UDP con puertos consecutivos**:
  - `base_port` — socket de **envío** (registro, mensajes, comandos; recibe ACK y
    respuestas a comandos).
  - `base_port + 1` — socket de **recepción** (recibe los mensajes difundidos por el
    servidor).

### 1.2 Naturaleza del servicio

El servidor actúa como **retransmisor sin estado de conversación**: no interpreta el
contenido   de los mensajes, sólo los reenvía. El servicio que ofrece es:

1. **Registro** de usuarios (alta en la tabla).
2. **Difusión** de mensajes a todos los registrados, con **asentimiento** al emisor.
3. **Consulta** de la lista de usuarios conectados.
4. **Baja** voluntaria de usuarios.

### 1.3 Intercambios desarrollados

| Intercambio | Sentido | Socket destino |
|---|---|---|
| Registro | Cliente → Servidor (puerto registro) | `REG` → `REG_OK` / `REG_ERR` |
| Mensaje de chat | Cliente → Servidor (puerto difusión) → todos | `MSG` → difusión `MSG` + `ACK` al emisor |
| Lista de usuarios | Cliente → Servidor (puerto difusión) | `LIST` → `LIST_OK` |
| Baja voluntaria | Cliente → Servidor (puerto difusión) | `QUIT` → `QUIT_OK` |

### 1.4 Infraestructura de comunicaciones

- **Protocolo de transporte:** UDP (`SOCK_DGRAM`, `IPPROTO_UDP`).
- **Familia de direcciones:** IPv4 (`AF_INET`).
- **Formato de mensajes:** texto ASCII con campos separados por `|`
  (ver `docs/protocolo.md`). Como UDP conserva los límites de datagrama, no se incluye
  campo de longitud. Tamaño máximo de datagrama: `1024` bytes
  (`protocol::MAX_DATAGRAM_SIZE`).

### 1.5 Tipo de servidores empleados

- **Servidor retransmisor:** servidor **multiservicio iterativo**. Un único hilo
  atiende los dos sockets (registro y difusión) multiplexándolos con `select()`. Cada
  petición se procesa de forma completa antes de pasar a la siguiente (iterativo).
- **Usuario:** servidor **iterativo** desde el punto de vista de la recepción (un hilo
  receptor procesa los datagramas entrantes uno a uno).

### 1.6 Hilos y procesos que intervienen

- **Servidor:** un único proceso, **un solo hilo** (bucle con `select()`). No necesita
  concurrencia porque el modelo es iterativo y las operaciones son no bloqueantes
  gracias a la multiplexación.
- **Cliente:** un proceso con **dos hilos**:
  - *Hilo principal:* lee la entrada de teclado, envía mensajes/comandos y gestiona el
    mecanismo de **timeout + reintentos** esperando el ACK.
  - *Hilo receptor (`receiverLoop`):* escucha el socket de recepción y muestra por
    pantalla los mensajes difundidos por el servidor.
  - La salida estándar se protege con un **mutex (`coutMutex`)** para evitar mezcla de
    texto entre ambos hilos.

### 1.7 Protocolos utilizados

Protocolo de aplicación propio de texto sobre UDP. Mensajes:

```txt
REG|username|recv_port          REG_OK|username        REG_ERR|reason
MSG|msg_id|username|text         ACK|msg_id
LIST|username                    LIST_OK|user1,user2,...
QUIT|username                    QUIT_OK|username
```

Mecanismo de **fiabilidad de extremo a extremo** sobre UDP: el emisor reenvía el mismo
`msg_id` hasta `MAX_RETRIES` (3) veces si no recibe `ACK` dentro de `ACK_TIMEOUT_MS`
(2000 ms). Para que esos reenvíos no generen mensajes duplicados, el servidor
**deduplica** por `(usuario, msg_id)`: si recibe un `msg_id` ya difundido por ese
usuario, no lo vuelve a difundir y sólo reenvía el `ACK`. Además, las **altas y
bajas** de usuarios se anuncian a todos mediante un mensaje de sistema
(`MSG|0|Servidor|...`), reutilizando el propio mecanismo de difusión.

### 1.8 Procedimientos y clases implementados

**Módulo común (`common/protocol.*`)** — espacio de nombres `protocol`:

- `split`, `getMessageType`, `isValidUsername` — utilidades de parseo y validación.
- `makeReg/parseReg`, `makeRegOk`, `makeRegErr` — registro.
- `makeMsg/parseMsg` — mensajes de chat (reconstruye el texto aunque contenga `|`;
  limpia `\n`/`\r`).
- `makeAck/parseAck` — asentimientos.
- `makeList/parseList`, `makeListOk/parseListOk` — listado de usuarios.
- `makeQuit/parseQuit`, `makeQuitOk/parseQuitOk` — baja voluntaria.
- Constantes: `MAX_DATAGRAM_SIZE`, `MAX_USERNAME_SIZE`, `ACK_TIMEOUT_MS`, `MAX_RETRIES`.
- `enum class MessageType` — tipos de mensaje del protocolo.

**Servidor (`server/server.cpp`)**:

- `struct User { username; recvAddr; lastMsgId; }` y `std::vector<User>` como tabla de
  usuarios (`lastMsgId` guarda el último `msg_id` difundido para deduplicar).
- `initWinsock`, `createUdpSocket`, `addrToString`, `sendDatagram`, `receiveDatagram`.
- `registerUser` (devuelve si el usuario es nuevo) / `removeUser` / `getUsernames` /
  `printUserTable` — gestión de tabla.
- `broadcastSystemMessage` — difunde avisos de alta/baja (`MSG|0|Servidor|...`).
- `handleRegistration` — atiende el puerto de registro y anuncia las altas nuevas.
- `handleChatMessage` — atiende el puerto de difusión (`MSG` con **deduplicación**,
  `LIST`, `QUIT` con aviso de baja).
- `shouldDropAck` — simulación de pérdida de ACK para probar reenvíos.
- `main` — bucle `select()` sobre los dos sockets.

**Cliente (`client/client.cpp`)**:

- `initWinsock`, `createBoundUdpSocket`, `makeServerAddr`, `sendDatagram`,
  `waitForDatagram`.
- `registerInServer` — registro y espera de `REG_OK`.
- `waitForAck` — espera del `ACK` del mensaje enviado.
- `receiverLoop` — hilo de recepción de mensajes difundidos.
- `requestUserList` (`/list`), `sendQuit` (`/quit`).
- `main` — registro, lanzamiento del hilo receptor y bucle de envío con reintentos.

### 1.9 Librerías usadas

- **Winsock 2** (`winsock2.h`, `ws2tcpip.h`, `Ws2_32.lib`) — sockets UDP en Windows.
- **STL:** `<string>`, `<vector>`, `<sstream>`, `<algorithm>`, `<thread>`, `<mutex>`,
  `<atomic>`, etc.

---

## 2. Manual de usuario

### 2.1 Compilación

**Con MinGW (g++):**

```bat
build_mingw.bat
```

**Con MSVC** (desde *Developer Command Prompt for Visual Studio*):

```bat
build_msvc.bat
```

Ambos generan `bin\server.exe` y `bin\client.exe`.

### 2.2 Arranque del servidor

```bat
run_server.bat                 :: equivale a: bin\server.exe 40000 40001
```

Parámetros: `server.exe <reg_port> <data_port> [drop_ack_percent]`

- `reg_port` — puerto de registro (por defecto 40000).
- `data_port` — puerto de difusión (por defecto 40001).
- `drop_ack_percent` — porcentaje de ACKs descartados artificialmente (para probar
  reintentos). `run_server_drop_ack.bat` lo lanza con 50 %.

Mensajes de consola del servidor: `[REG RX]`, `[REG]`, `[USERS]` (tabla de usuarios),
`[DATA RX]`, `[MSG TX]`, `[ACK TX]`/`[ACK DROP]`, `[LIST TX]`, `[QUIT]`.

### 2.3 Arranque de un cliente

```bat
run_client_pablo.bat           :: bin\client.exe Pablo 127.0.0.1 40000 40001 50000
run_client_ana.bat             :: bin\client.exe Ana   127.0.0.1 40000 40001 50002
run_client_luis.bat            :: bin\client.exe Luis  127.0.0.1 40000 40001 50004
```

Parámetros: `client.exe <usuario> <server_ip> <reg_port> <data_port> <base_port>`

El cliente usa `base_port` para envío y `base_port + 1` para recepción (puertos
consecutivos). Por eso cada cliente reserva un hueco de **2 puertos** (50000/50001,
50002/50003, 50004/50005).

### 2.4 Uso del cliente

- Escribir un texto y pulsar **ENTER** para enviarlo al chat. El cliente muestra
  `[MSG TX] intento N`, y `[ACK RX]` cuando el servidor confirma. Si no llega el ACK,
  muestra `[TIMEOUT]` y reintenta (hasta 3 veces).
- Los mensajes de otros usuarios aparecen como `[usuario #id] texto`.
- **Comandos:**
  - `/list` — muestra los usuarios registrados en el servidor.
  - `/quit` — se da de baja del servidor y cierra el cliente.

### 2.5 Prueba del mecanismo de timeout/reenvío

1. Arrancar el servidor con pérdida simulada: `run_server_drop_ack.bat`.
2. Arrancar uno o varios clientes y enviar mensajes.
3. Observar en el cliente los `[TIMEOUT]` y los reintentos; en el servidor, los
   `[ACK DROP]`.

---

## 3. Pruebas realizadas

| Prueba | Resultado esperado |
|---|---|
| Registro de un usuario | Servidor muestra `[REG] Usuario nuevo` y tabla `[USERS] 1` |
| Registro de varios usuarios | Tabla con todos los usuarios |
| Mensaje entre dos clientes | Ambos reciben `[usuario #id] texto`; emisor recibe `[ACK RX]` |
| `/list` | El cliente lista los usuarios conectados |
| `/quit` | El usuario desaparece de la tabla del servidor |
| Servidor con `drop_ack_percent=50` | El cliente muestra `[TIMEOUT]` y reintenta |

---

## 4. Conclusiones

En este proyecto hemos implementado el servicio conversacional multiusuario
mediante protocolo UDP, dos sockets consecutivos por usuario, entidad retransmisora
multiservicio iterativa con `select()`, tabla de usuarios, difusión a todos
los registrados y asentimiento al emisor. Además, hemos añadido un mecanismo
de **fiabilidad (timeout + reintentos)** sobre UDP y una **simulación de pérdida
de ACKs** para demostrarlo, **deduplicación** en el servidor para que esos reenvíos
no produzcan mensajes repetidos, **avisos de presencia** (altas y bajas) y los
comandos `/list` y `/quit`.

