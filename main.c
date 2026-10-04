#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "config_pines.h"
#include "shared_resources.h"
#include "i2c_bus.h"
#include "ds3231.h"
#include "termocuplas.h"
#include "sd_logger.h"
#include "serial_logger.h"
#include "oled_display.h"
#include "roast_control.h"
#include "web_server.h"


static const char *TAG = "MAIN";


/* Prioridades: lectura de sensores y guardado en SD son la prioridad
 * del sistema. RTC es intermedio (lo necesita el sensor). Serial y
 * OLED son las de menor prioridad: informativas, no críticas. */
#define PRIO_SENSOR  6
#define PRIO_SD      6
#define PRIO_RTC     5
#define PRIO_SERIAL  3
#define PRIO_OLED    2


static esp_err_t spi_bus_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = PIN_NUM_MOSI,
        .miso_io_num     = PIN_NUM_MISO,
        .sclk_io_num     = PIN_NUM_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };
    return spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO);
}


void app_main(void)
{
    if (!shared_resources_init()) {
        ESP_LOGE(TAG, "No se pudieron crear mutex/colas");
        return;
    }


    /* --- Bus I2C: DS3231 + OLED --- */
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(ds3231_init());
    ESP_ERROR_CHECK(oled_display_init());


    /* --- Bus SPI: MAX6675 (con mux CD4052) + microSD --- */
    ESP_ERROR_CHECK(spi_bus_init());
    ESP_ERROR_CHECK(termocuplas_init());
    ESP_ERROR_CHECK(roast_control_init());


    if (sd_logger_init() != ESP_OK) {
        ESP_LOGW(TAG, "Continuando sin SD: los datos solo saldrán por serial/OLED");
    }


    /* --- Tareas --- */
    xTaskCreate(vTaskSensorTermocuplas,"sensor_task", 4096, NULL, PRIO_SENSOR, NULL);
    xTaskCreate(vTaskSdWriter,"sd_task",     6144, NULL, PRIO_SD,     NULL);
    xTaskCreate(vTaskRtc, "rtc_task",    3072, NULL, PRIO_RTC,    NULL);
    xTaskCreate(vTaskSerialSender,"serial_task", 3072, NULL, PRIO_SERIAL, NULL);
    xTaskCreate(vTaskOled,    "oled_task",   4096, NULL, PRIO_OLED,   NULL);
    xTaskCreate(vTaskRoast,  "roast_task", 3072, NULL, 4, NULL);

    if (web_server_init() != ESP_OK) {
        ESP_LOGW(TAG, "WiFi/web no disponible: el sistema sigue funcionando sin pagina");
    }
    ESP_LOGI(TAG, "Datalogger iniciado (SD %s)",
             sd_logger_disponible() ? "OK" : "NO disponible");
}
