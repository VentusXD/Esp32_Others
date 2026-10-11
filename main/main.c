#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

// Power Management CPU frequency scaling and sleep modes
#include "esp_sleep.h"
//#include <dht.h> // https://components.espressif.com/components/esp-idf-lib/dht/versions/1.2.0/readme  version oficial 1.2.0

#include "mqtt_tb.h"
#include "wifi_provisioning.h"
#include "dht11.h"
#include "ldr.h"

static const char *TAG = "TB_MAIN";
static const char *TAGW = "WIFI_PROV";

dht11_t sensor = {
        .dht11_pin = GPIO_NUM_4,  // PIN
        .temperature = 0.0f,
        .humidity = 0.0f
};


void dht_task(void *param) {
    gpio_reset_pin(sensor.dht11_pin); // Resetear el pin antes de configurarlo
    gpio_set_pull_mode(sensor.dht11_pin, GPIO_PULLUP_ONLY);
    while (1) {
        int result = dht11_read(&sensor, 5); // 5 intentos de conexion
        if (result == 0) { // -1 fallo, 0 exito
            ESP_LOGI(TAG, "Temperatura: %.2f °C, Humedad: %.2f %%", sensor.temperature, sensor.humidity);
            tb_mqtt_publish_float("temperature", sensor.temperature);
            tb_mqtt_publish_float("humidity", sensor.humidity);
        } else {
            ESP_LOGE(TAG, "Error al leer el sensor DHT11");
        }
        ESP_LOGI(TAG, "Esperando %i s", rpc_get_valor());  // Comprobar RPC setValor
        vTaskDelay(pdMS_TO_TICKS(rpc_get_valor()*1000)); // Esperar volatil RPC setValor
    }   // por limitacion de thingboard por lo que no hace falta usar xqueue
}


void app_main(void) {
    //nvs_flash_erase(); Borrado de config anterior 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    wifi_provisioning_init();
    //ble_simple_init();  // Blue muerto
    wifi_provisioning_start_webserver();

    wifi_prov_info_t info;
    wifi_provisioning_get_status(&info);

    if (info.status == WIFI_PROV_STATUS_CONNECTED) {  // Usar para saber si hay o no wifi
            ESP_LOGI(TAGW, "SSID: %s, IP: %s", info.ssid, info.ip_str);
            
    } else if (info.status == WIFI_PROV_STATUS_FAILED) {
            ESP_LOGW(TAGW, "Fallo al conectar a '%s': %s", info.ssid, info.reason);
    }
    vTaskDelay(pdMS_TO_TICKS(3000)); // Esperar 1 segundo antes de continuar
    
    tb_mqtt_start(); 
    xTaskCreate(dht_task, "dht_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

    xTaskCreate(ldr_task, "ldr_task", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL);



}