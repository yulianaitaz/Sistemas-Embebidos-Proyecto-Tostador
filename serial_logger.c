#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "serial_logger.h"
#include "shared_resources.h"
#include "data_types.h"

void vTaskSerialSender(void *pvParameters)
{
    datalog_entry_t d;
    char linea[160];


    for (;;) {
        if (xQueueReceive(xSerialQueue, &d, portMAX_DELAY) != pdTRUE) continue;
        datalog_format_csv(linea, sizeof(linea), &d);


        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            printf("%s\n", linea);
            if (d.alerta == ALERTA_TEMP_ALTA || d.alerta == ALERTA_TEMP_BAJA ||
                d.alerta == ALERTA_DESCONECTADA) {
                printf("!!! ALERTA %s (%.1f C)\n", datalog_nombre_sensor(d.sensor_id), d.temp_celsius);
            }
            xSemaphoreGive(xSerialMutex);
        }
    }
}
