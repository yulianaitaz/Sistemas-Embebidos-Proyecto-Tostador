#include <stdio.h>
#include "esp_log.h"
#include "shared_resources.h"
#include "roast_control.h"


static const char *TAG = "SHARED";


#define QUEUE_LEN 10


SemaphoreHandle_t xSpiMutex    = NULL;
SemaphoreHandle_t xI2cMutex    = NULL;
SemaphoreHandle_t xSerialMutex = NULL;


QueueHandle_t xSdQueue     = NULL;
QueueHandle_t xSerialQueue = NULL;
QueueHandle_t xOledQueue   = NULL;


static datalog_entry_t s_ultimo[2];
static bool            s_hay[2];
static portMUX_TYPE    s_ult_mux = portMUX_INITIALIZER_UNLOCKED;

bool shared_get_ultimo(sensor_id_t id, datalog_entry_t *out)
{
    bool ok;
    taskENTER_CRITICAL(&s_ult_mux);
    ok = s_hay[id];
    if (ok) *out = s_ultimo[id];
    taskEXIT_CRITICAL(&s_ult_mux);
    return ok;
}

bool shared_resources_init(void)
{
    xSpiMutex    = xSemaphoreCreateMutex();
    xI2cMutex    = xSemaphoreCreateMutex();
    xSerialMutex = xSemaphoreCreateMutex();


    xSdQueue     = xQueueCreate(QUEUE_LEN, sizeof(datalog_entry_t));
    xSerialQueue = xQueueCreate(QUEUE_LEN, sizeof(datalog_entry_t));
    xOledQueue   = xQueueCreate(QUEUE_LEN, sizeof(datalog_entry_t));


    return xSpiMutex && xI2cMutex && xSerialMutex &&
           xSdQueue && xSerialQueue && xOledQueue;
}


void cola_publish(const datalog_entry_t *entry)
{
    if (entry->sensor_id < 2) {
        taskENTER_CRITICAL(&s_ult_mux);
        s_ultimo[entry->sensor_id] = *entry;
        s_hay[entry->sensor_id] = true;
        taskEXIT_CRITICAL(&s_ult_mux);
    }
    if (xQueueSend(xSdQueue, entry, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Cola SD llena, muestra descartada");
    }
    if (xQueueSend(xSerialQueue, entry, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Cola serial llena, muestra descartada");
    }
    /* El OLED solo necesita el ultimo dato de cada sensor: si su cola
     * esta llena, se descarta el más viejo y se mete el nuevo, sin
     * bloquear jamas a la tarea productora. */
    if (xQueueSend(xOledQueue, entry, 0) != pdTRUE) {
        datalog_entry_t descartado;
        xQueueReceive(xOledQueue, &descartado, 0);
        xQueueSend(xOledQueue, entry, 0);
    }
}


const char *datalog_nombre_sensor(sensor_id_t id)
{
    return (id == SENSOR_CAFE) ? "CAFE" : "TAMBOR";
}


const char *datalog_texto_estado(estado_sensor_t e)
{
    return (e == ESTADO_OK) ? "OK" : "TERMOCUPLA DESCONECTADA";


}


static const char *texto_alerta(alerta_t a)
{
    switch (a) {
        case ALERTA_CALENTANDO:   return "CALENTANDO";
        case ALERTA_TEMP_BAJA:    return "TEMP_BAJA";
        case ALERTA_TEMP_ALTA:    return "TEMP_ALTA";
        case ALERTA_DESCONECTADA: return "DESCONECTADA";
        default:                  return "OK";
    }
}


void datalog_format_csv(char *out, size_t n, const datalog_entry_t *e)
{
    snprintf(out, n, "%llu,%s,%s,%.2f,%s,%s,%s,%lu",
             (unsigned long long)e->timestamp_ms, e->datetime,
             datalog_nombre_sensor(e->sensor_id), e->temp_celsius,
             datalog_texto_estado(e->estado),
             roast_nombre_tueste(e->tueste), texto_alerta(e->alerta),
             (unsigned long)e->t_tueste_s);
}
