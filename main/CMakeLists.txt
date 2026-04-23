idf_component_register(
    SRCS 
        "main.c"
        "interface/src/interface.c"
        "interface/src/srv_wifi.c"
        "Protocols/src/protocol.c"
        "Protocols/src/mqtt.c"
        "drivers/src/drv_gpio.c"

    INCLUDE_DIRS 
        "."
        "interface/inc"
        "Protocols/inc"
        "drivers/inc"

    REQUIRES 
        esp_netif
        esp_wifi
        esp_event
        nvs_flash
        esp-mqtt
        esp_http_client
        driver
)