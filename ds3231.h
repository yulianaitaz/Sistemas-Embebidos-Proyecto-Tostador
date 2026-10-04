#ifndef DS3231_H
#define DS3231_H


#include <stddef.h>
#include "esp_err.h"


/* Agrega el DS3231 al bus I2C (creado por i2c_bus_init()). */
esp_err_t ds3231_init(void);


/* "YYYY-MM-DD HH:MM:SS" en out (out debe medir >= DATETIME_LEN).
 * */
void ds3231_get_datetime_str(char *out, size_t n);

/* Tarea: lee el DS3231 cada 1 s (protegida con xI2cMutex) y actualiza
 * el timestamp interno que consultan las demas tareas. */
void vTaskRtc(void *pvParameters);


#endif /* DS3231_H */
