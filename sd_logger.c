#include <stdio.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "sd_logger.h"
#include "config_pines.h"
#include "shared_resources.h"
#include "data_types.h"


static const char *TAG = "SD";


#define MOUNT_POINT "/sdcard"
#define LOG_PATH    MOUNT_POINT "/datalog.csv"
#define CSV_HEADER  "timestamp_ms,datetime,sensor,temperatura,estado,tueste,alerta,t_tueste_s"

static volatile uint32_t s_guardadas = 0, s_errores = 0;
static volatile bool     s_ultimo_ok = true;
static sdmmc_card_t *s_card;
static bool s_ok = false;


esp_err_t sd_logger_init(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 4,
        .allocation_unit_size   = 16 * 1024,
    };


    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot         = SPI_HOST_ID;
    host.max_freq_khz = 10000;             /* 10 MHz: más estable con cables */


    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = PIN_NUM_CS_SD;
    slot_cfg.host_id = SPI_HOST_ID;


    esp_err_t err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo montar la SD (%s)", esp_err_to_name(err));
        s_ok = false;
        return err;
    }
    s_ok = true;
    sdmmc_card_print_info(stdout, s_card);


    struct stat st;
    if (stat(LOG_PATH, &st) != 0 || st.st_size == 0) {
        if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            FILE *f = fopen(LOG_PATH, "a");
            if (f) { fprintf(f, "%s\n", CSV_HEADER); fclose(f); }
            else ESP_LOGE(TAG, "No se pudo crear %s", LOG_PATH);
            xSemaphoreGive(xSpiMutex);
        }
    }
    return ESP_OK;
}


bool sd_logger_disponible(void) { return s_ok; }

void sd_logger_get_stats(bool *ultimo_ok, uint32_t *guardadas, uint32_t *errores)
{
    *ultimo_ok = s_ultimo_ok;
    *guardadas = s_guardadas;
    *errores   = s_errores;
}

void vTaskSdWriter(void *pvParameters)
{
    datalog_entry_t data;
    char linea[160];


    for (;;) {
        if (xQueueReceive(xSdQueue, &data, portMAX_DELAY) != pdTRUE) continue;


        if (!s_ok) {
            ESP_LOGE(TAG, "SD no disponible, muestra no guardada");
            continue;
        }


        datalog_format_csv(linea, sizeof(linea), &data);


        if (xSemaphoreTake(xSpiMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            bool ok = false;
            FILE *f = fopen(LOG_PATH, "a");
            if (f) {
                ok = fprintf(f, "%s\n", linea) > 0;
                ok = (fclose(f) == 0) && ok;     /* cierra y vuelca a la tarjeta */
            } else {
                ESP_LOGE(TAG, "Error de escritura en la SD");
            }
            xSemaphoreGive(xSpiMutex);
            if (ok) s_guardadas++; else s_errores++;
            s_ultimo_ok = ok;
        } else {
            ESP_LOGE(TAG, "Timeout esperando bus SPI para escribir en la SD");
            s_errores++;
            s_ultimo_ok = false;
        }
    }
}
