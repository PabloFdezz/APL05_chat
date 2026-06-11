# Plan de trabajo ajustado por hitos

## Hito 1 - UDP básico con Winsock

Estado: completado.

Objetivo: compilar y ejecutar servidor/cliente con UDP.

Criterio: el servidor recibe un datagrama y lo muestra.

## Hito 2 - Registro

Estado: completado.

Objetivo: cliente envía `REG`, servidor guarda usuario y responde `REG_OK`.

Criterio: la tabla del servidor muestra usuarios registrados.

## Hito 3 - Difusión

Estado: completado.

Objetivo: cliente envía `MSG`, servidor lo reenvía a todos los registrados.

Criterio: dos o más clientes reciben mensajes entre sí.

## Hito 4 - ACK

Estado: completado.

Objetivo: servidor confirma cada mensaje al remitente.

Criterio: cliente muestra `ACK RX`.

## Hito 5 - Timeout y reenvío

Estado: completado.

Objetivo: cliente reenvía si no llega ACK.

Criterio: al simular pérdida, el cliente realiza varios intentos.

## Hito 6 - Mejoras útiles

Estado: parcialmente completado.

Implementado:

- `/list`
- `/quit`
- simulación de pérdida de ACK
- logs más claros de tabla de usuarios
- deduplicación de mensajes reenviados (servidor filtra `(usuario, msg_id)`)
- avisos de presencia (altas y bajas) mediante mensaje de sistema

Pendiente opcional:

- timestamps
- logs a fichero

## Hito 7 - Memoria y defensa

Estado: memoria redactada en `docs/MEMORIA.md`. Pendiente: rellenar
`docs/datos_grupo.txt` y empaquetar la entrega.

Contenido de la memoria:

- arquitectura
- protocolo
- servidor
- cliente
- pruebas
- manual de usuario
- conclusiones
