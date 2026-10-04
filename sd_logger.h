#ifndef SD_LOGGER_H
#define SD_LOGGER_H
#include <stdbool.h>
#include "esp_err.h"
#include <stdint.h>


/* Monta la microSD (sobre el bus SPI ya inicializado en main.c) y
 * crea el archivo con encabezado si no existe. */
esp_err_t sd_logger_init(void);
bool sd_logger_disponible(void);

/* Tarea de alta prioridad: toma cada muestra de xSdQueue y la agrega
 * al CSV en la microSD. */
void vTaskSdWriter(void *pvParameters);
/* Estado del guardado: ultimo_ok = la ultima escritura funciono */
void sd_logger_get_stats(bool *ultimo_ok, uint32_t *guardadas, uint32_t *errores);

#endif /* SD_LOGGER_H */
