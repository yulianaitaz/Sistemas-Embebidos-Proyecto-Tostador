#ifndef SHARED_RESOURCES_H
#define SHARED_RESOURCES_H


#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "data_types.h"


/* --- Mutex de recursos físicos compartidos --- */
extern SemaphoreHandle_t xSpiMutex;     /* bus SPI: MAX6675 + microSD          */
extern SemaphoreHandle_t xI2cMutex;     /* bus I2C: DS3231 + OLED               */
extern SemaphoreHandle_t xSerialMutex;  /* consola / UART                       */


/* --- "Cola" del diagrama: una FreeRTOS queue por tarea consumidora,
 *     porque una queue solo entrega cada dato a UN receptor. --- */
extern QueueHandle_t xSdQueue;
extern QueueHandle_t xSerialQueue;
extern QueueHandle_t xOledQueue;


/* Crea todos los mutex y colas. Llamar una sola vez desde app_main(). */
bool shared_resources_init(void);


/* Publica una muestra en las 3 colas de salida (SD, Serial, OLED). */
void cola_publish(const datalog_entry_t *entry);


/* Utilidades de formato, usadas por sd_logger y serial_logger. */
const char *datalog_nombre_sensor(sensor_id_t id);
const char *datalog_texto_estado(estado_sensor_t e);
void datalog_format_csv(char *out, size_t n, const datalog_entry_t *e);

/* Última muestra de cada sensor (para la página web). false si aún no hay. */
bool shared_get_ultimo(sensor_id_t id, datalog_entry_t *out);

#endif /* SHARED_RESOURCES_H */
