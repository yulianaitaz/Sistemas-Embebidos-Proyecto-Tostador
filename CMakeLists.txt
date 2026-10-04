idf_component_register(
    SRCS
        "main.c"
        "shared_resources.c"
        "i2c_bus.c"
        "roast_control.c"
        "ds3231.c"
        "mux_cd4052.c"
        "termocuplas.c"
        "sd_logger.c"
        "serial_logger.c"
        "oled_display.c"
        "rgb_led.c"
        "motor_l298n.c"
        "web_server.c"
        "heater_relay.c"
    
    INCLUDE_DIRS "."
    EMBED_FILES "index.html"
    PRIV_REQUIRES driver fatfs sdmmc esp_timer
                  esp_wifi esp_netif esp_event nvs_flash esp_http_server
)

