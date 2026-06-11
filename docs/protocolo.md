# Protocolo de comunicación APL05

## Formato general

El protocolo usa mensajes de texto separados por `|`.

UDP conserva los límites de datagrama, por lo que no se incluye campo de longitud en esta versión inicial.

Tamaño máximo recomendado de datagrama:

```txt
1024 bytes
```

## Registro

Cliente -> servidor, puerto de registro:

```txt
REG|username|recv_port
```

Ejemplo:

```txt
REG|Pablo|50001
```

Servidor -> cliente:

```txt
REG_OK|Pablo
```

Error:

```txt
REG_ERR|Formato REG invalido
```

## Mensaje de chat

Cliente -> servidor, puerto de difusión:

```txt
MSG|msg_id|username|text
```

Ejemplo:

```txt
MSG|1|Pablo|Hola a todos
```

Servidor -> clientes registrados:

```txt
MSG|1|Pablo|Hola a todos
```

## ACK

Servidor -> remitente:

```txt
ACK|msg_id
```

Ejemplo:

```txt
ACK|1
```

## Lista de usuarios

Cliente -> servidor, puerto de difusión:

```txt
LIST|username
```

Servidor -> cliente:

```txt
LIST_OK|Pablo,Ana,Luis
```

## Salida voluntaria

Cliente -> servidor, puerto de difusión:

```txt
QUIT|username
```

Servidor -> cliente:

```txt
QUIT_OK|username
```

## Mensajes de sistema (altas y bajas)

Cuando un usuario se da de alta o de baja, el servidor difunde a todos los
usuarios registrados un mensaje de chat normal con remitente `Servidor` e id `0`:

```txt
MSG|0|Servidor|Pablo se ha unido al chat
MSG|0|Servidor|Pablo ha salido del chat
```

No requiere ningun cambio en el cliente: se muestran como un mensaje mas
(`[Servidor #0] Pablo se ha unido al chat`).

## Decisiones de diseño

- El servidor reenvía los mensajes a todos los usuarios registrados, incluido el emisor.
- El cliente usa dos puertos consecutivos:
  - `base_port`: envío
  - `base_port + 1`: recepción
- El ACK llega al socket de envío del cliente.
- Los mensajes de chat llegan al socket de recepción del cliente.
- El cliente reintenta el envío si no recibe ACK tras un timeout.
- `/list` consulta la tabla de usuarios del servidor.
- `/quit` elimina al usuario de la tabla de usuarios del servidor.
- La pérdida simulada de ACKs permite probar el mecanismo de timeout y reenvío.
- **Deduplicación:** el servidor guarda el último `msg_id` difundido de cada
  usuario. Si recibe un `MSG` con un `msg_id` que ya difundió (reenvío del cliente
  por pérdida de ACK), no lo vuelve a difundir y se limita a reenviar el `ACK`,
  que es lo que probablemente se perdió. Así los destinatarios no ven duplicados.
  El contador se reinicia cuando el usuario vuelve a registrarse (nueva sesión).
- **Avisos de presencia:** las altas y bajas se anuncian a todos los registrados
  mediante un mensaje de sistema (`MSG|0|Servidor|...`).
