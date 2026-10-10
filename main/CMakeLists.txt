idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt wifi_provisioning nvs_flash mqtt_tb dht11 ldr rpc) 
