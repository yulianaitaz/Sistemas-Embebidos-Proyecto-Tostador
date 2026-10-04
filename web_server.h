#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* Conecta el ESP32 a la red WiFi configurada en menuconfig (modo cliente)
 * y arranca el servidor HTTP. No bloquea esperando la conexion. */
esp_err_t web_server_init(void);

/* Copia la IP actual en out ("192.168.43.25"). Devuelve false si todavia
 * no esta conectado a la red. */
bool web_server_get_ip(char *out, size_t n);

#endif
