idf_component_register(
    SRCS 
        "main.c"
        "interface/src/interface.c"
        "interface/src/srv_wifi.c"
        "Protocols/src/protocol.c"
        "Protocols/src/mqtt.c"
        "Protocols/src/http.c"
        "drivers/src/drv_gpio.c"

    INCLUDE_DIRS 
        "."
        "interface/inc"
        "Protocols/inc"
        "drivers/inc"

    REQUIRES 
        cjson
        esp_netif
        esp_wifi
        esp_event
        nvs_flash
        mqtt
        esp_http_client
        esp_http_server
        driver
        esp_system
)