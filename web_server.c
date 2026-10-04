#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "sdkconfig.h"
#include "heater_relay.h"

#include "web_server.h"
#include "shared_resources.h"
#include "roast_control.h"
#include "motor_l298n.h"
#include "ds3231.h"
#include "data_types.h"

#define WIFI_SSID  CONFIG_TOSTADOR_WIFI_SSID
#define WIFI_PASS  CONFIG_TOSTADOR_WIFI_PASSWORD

#define CHK(x) do { esp_err_t e_ = (x); if (e_ != ESP_OK) return e_; } while (0)

static const char *TAG = "WEB";

static char s_ip[16] = "";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/* ---------------- WiFi (modo cliente) ---------------- */

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Conectando a '%s'...", WIFI_SSID);
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip[0] = '\0';
        ESP_LOGW(TAG, "Sin conexion con '%s', reintentando...", WIFI_SSID);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "Conectado. Abre en el navegador: http://%s", s_ip);
    }
}

bool web_server_get_ip(char *out, size_t n)
{
    if (s_ip[0] == '\0') return false;
    snprintf(out, n, "%s", s_ip);
    return true;
}

static esp_err_t wifi_init_sta(void)
{
    CHK(esp_netif_init());
    CHK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    CHK(esp_wifi_init(&wcfg));

    CHK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    CHK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    wifi_config_t sta = { 0 };
    strlcpy((char *)sta.sta.ssid, WIFI_SSID, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, WIFI_PASS, sizeof(sta.sta.password));
    sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;     /* acepta WPA2 y WPA3 */
    sta.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    CHK(esp_wifi_set_mode(WIFI_MODE_STA));
    CHK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    CHK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);                       /* respuesta mas rapida */
    return ESP_OK;
}

/* ---------------- Servidor HTTP ---------------- */

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, (const char *)index_html_start,
                           index_html_end - index_html_start);
}

static int json_sensor(char *o, size_t n, sensor_id_t id)
{
    datalog_entry_t d;
    if (!shared_get_ultimo(id, &d)) return snprintf(o, n, "{\"hay\":0}");
    return snprintf(o, n, "{\"hay\":1,\"ok\":%d,\"temp\":%.1f,\"alerta\":%d}",
                    d.estado == ESTADO_OK, d.temp_celsius, (int)d.alerta);
}

static esp_err_t api_estado(httpd_req_t *req)
{
    roast_info_t ri;
    roast_get_info(&ri);

    char hora[DATETIME_LEN], c[96], t[96], buf[512];
    ds3231_get_datetime_str(hora, sizeof(hora));
    json_sensor(c, sizeof(c), SENSOR_CAFE);
    json_sensor(t, sizeof(t), SENSOR_TAMBOR);

    snprintf(buf, sizeof(buf),
        "{\"hora\":\"%s\",\"tueste\":%d,\"nombre\":\"%s\",\"tmin\":%.0f,\"tmax\":%.0f,"
        "\"t_min_s\":%u,\"t_max_s\":%u,\"elapsed\":%u,\"fase\":%d,\"motor\":%d,\"calor\":%d,"
        "\"fan\":%d,\"cafe\":%s,\"tambor\":%s}",
        hora, (int)ri.tueste, ri.nombre, ri.temp_min, ri.temp_max,
        (unsigned)ri.t_min_s, (unsigned)ri.t_max_s, (unsigned)ri.elapsed_s,
        (int)ri.fase, motor_esta_girando(), rele_esta_encendido(), (int)ri.fan , c, t);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, buf);
}

static esp_err_t api_start(httpd_req_t *req)
{
    bool run = roast_toggle();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, run ? "{\"run\":1}" : "{\"run\":0}");
}

static esp_err_t api_tueste(httpd_req_t *req)
{   
    char q[16], v[4];
    int n = -1;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "n", v, sizeof(v)) == ESP_OK) {
        n = atoi(v);
    }
    bool ok = roast_set_tueste(n);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, ok ? "{\"ok\":1}" : "{\"ok\":0}");
}

static esp_err_t api_fan(httpd_req_t *req)
{
    bool ok = roast_fan_test_toggle();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, ok ? "{\"ok\":1}" : "{\"ok\":0}");
}

static esp_err_t start_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.lru_purge_enable = true;
    httpd_handle_t srv = NULL;
    CHK(httpd_start(&srv, &cfg));

    httpd_uri_t u_root   = { .uri = "/",           .method = HTTP_GET,  .handler = root_get   };
    httpd_uri_t u_estado = { .uri = "/api/estado", .method = HTTP_GET,  .handler = api_estado };
    httpd_uri_t u_start  = { .uri = "/api/start",  .method = HTTP_POST, .handler = api_start  };
    httpd_uri_t u_tueste = { .uri = "/api/tueste", .method = HTTP_POST, .handler = api_tueste };
    httpd_register_uri_handler(srv, &u_root);
    httpd_register_uri_handler(srv, &u_estado);
    httpd_register_uri_handler(srv, &u_start);
    httpd_register_uri_handler(srv, &u_tueste);
    httpd_uri_t u_fan    = { .uri = "/api/fan",    .method = HTTP_POST, .handler = api_fan    };
    httpd_register_uri_handler(srv, &u_fan);
    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        r = nvs_flash_init();
    }
    CHK(r);

    CHK(wifi_init_sta());
    return start_server();
}