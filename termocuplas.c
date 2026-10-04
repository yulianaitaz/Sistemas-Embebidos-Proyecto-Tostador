#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "termocuplas.h"
#include "mux_cd4052.h"
#include "config_pines.h"
#include "shared_resources.h"
#include "data_types.h"
#include "ds3231.h"
#include "roast_control.h"


static const char *TAG = "TERMOCUPLAS";
static spi_device_handle_t s_max6675;


#define PERIODO_CAFE_MS    5000   /* cafe: cada ciclo   -> cada 5 s  */
#define CICLOS_POR_TAMBOR  2      /* tambor: cada 2 ciclos -> cada 10 s */


esp_err_t termocuplas_init(void)
{
    esp_err_t err = mux_cd4052_init();
    if (err != ESP_OK) return err;


    spi_device_interface_config_t max_cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,   /* máx. 4.3 MHz según datasheet */
        .mode           = 0,
        .spics_io_num   = PIN_NUM_CS_MAX6675,
        .queue_size     = 1,
    };
    return spi_bus_add_device(SPI_HOST_ID, &max_cfg, &s_max6675);
}


static esp_err_t max6675_leer_raw(float *temp_c, estado_sensor_t *estado)
{
    spi_transaction_t t = {
        .length = 16,
        .flags  = SPI_TRANS_USE_RXDATA,
    };
    esp_err_t err = spi_device_polling_transmit(s_max6675, &t);
    if (err != ESP_OK) {
        *estado = ESTADO_DESCONECTADA;
        *temp_c = 0.0f;
        return err;
    }


    uint16_t raw = ((uint16_t)t.rx_data[0] << 8) | t.rx_data[1];
    if (raw & 0x04) {                          /* bit D2 = termocupla abierta */
        *estado = ESTADO_DESCONECTADA;
        *temp_c = 0.0f;
    } else {
        *estado = ESTADO_OK;
        *temp_c = (raw >> 3) * 0.25f;
    }
    return ESP_OK;
}


/* Lee una termocupla (el canal del mux ya debe estar seleccionado) y
 * publica la muestra. El datetime del RTC SIEMPRE se agrega, incluso
 * si la termocupla está desconectada. */
static void leer_y_publicar(sensor_id_t id)
{
    datalog_entry_t e;
    memset(&e, 0, sizeof(e));
    e.sensor_id     = id;
    e.timestamp_ms  = esp_timer_get_time() / 1000;


    if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (max6675_leer_raw(&e.temp_celsius, &e.estado) != ESP_OK) {
            ESP_LOGW(TAG, "Fallo SPI leyendo %s", datalog_nombre_sensor(id));
            e.estado = ESTADO_DESCONECTADA;
        }
        xSemaphoreGive(xSpiMutex);
    } else {
        ESP_LOGW(TAG, "Timeout esperando bus SPI");
        e.estado = ESTADO_DESCONECTADA;
    }


    if (e.estado == ESTADO_DESCONECTADA) {
        ESP_LOGW(TAG, "%s: termocupla desconectada", datalog_nombre_sensor(id));
    }


    ds3231_get_datetime_str(e.datetime, sizeof(e.datetime));
    roast_info_t ri;
    roast_get_info(&ri);
    e.tueste     = ri.tueste;
    e.t_tueste_s = ri.elapsed_s;
    e.alerta     = roast_evaluar(id, e.temp_celsius, e.estado);
   
    cola_publish(&e);
}


void vTaskSensorTermocuplas(void *pvParameters)
{
    TickType_t xLastWake = xTaskGetTickCount();
    uint32_t ciclo = 0;


    for (;;) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(PERIODO_CAFE_MS));


        /* Cafe: prioridad. El mux normalmente ya está en este canal,
         * asi que no hay retardo de asentamiento la mayoría de veces. */
        mux_select_canal(MUX_CANAL_CAFE);
        leer_y_publicar(SENSOR_CAFE);


        if (++ciclo % CICLOS_POR_TAMBOR == 0) {
            mux_select_canal(MUX_CANAL_TAMBOR);
            leer_y_publicar(SENSOR_TAMBOR);
            /* Se deja el mux listo en café para el próximo ciclo, ahora
             * que no hay apuro de tiempo (el settle no afecta a nadie). */
            mux_select_canal(MUX_CANAL_CAFE);
        }
    }
}
