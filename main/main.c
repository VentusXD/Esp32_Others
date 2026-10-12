#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

// Power Management CPU frequency scaling and sleep modes
#include "esp_pm.h"
//#include <dht.h> // https://components.espressif.com/components/esp-idf-lib/dht/versions/1.2.0/readme  version oficial 1.2.0

#include "mqtt_tb.h"
#include "wifi_provisioning.h"
#include "dht11.h"
#include "ldr.h"

static const char *TAG = "TB_MAIN ";
static const char *TAGW = "WIFI_PROV ";

dht11_t sensor = {
        .dht11_pin = GPIO_NUM_4,  // PIN
        .temperature = 0.0f,
        .humidity = 0.0f
};

static esp_pm_lock_handle_t s_dht_lock;  // lock para dht11 uso MAX CPU FREQ

static void dht_task(void *param);
static void power_init(void); // politica de ahorro de energia


void app_main(void) {
    //nvs_flash_erase(); //Borrado de config anterior 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    power_init(); // Ahorro de energia

    wifi_provisioning_init();
    //ble_simple_init();  // Blue muerto
    wifi_provisioning_start_webserver();

    wifi_low_power_mode(); // bajo consumo (STA + modem sleep) Handler ENVENT HACER!!!


            
    tb_mqtt_start(); 
    
    //prueba RPC setValor() por Thingboard
    /*while(1){
        ESP_LOGI(TAG, "setvalor: %i" ,rpc_get_valor()); 
        vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar 2 segundos antes de volver a leer el valor
    }*/ 


    xTaskCreate(dht_task, "dht_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    
    xTaskCreate(ldr_task, "ldr_task", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL); 
    // pasar periodo de publicacion por parametro
    //xTaskCreate(ldr_task, "ldr_task", configMINIMAL_STACK_SIZE * 4, (void *)(uintptr_t)rpc_get_valor(), 5, NULL); 
    // INTENTO FALLIDO


}


void dht_task(void *param) {
    gpio_reset_pin(sensor.dht11_pin); // Resetear el pin antes de configurarlo
    gpio_set_pull_mode(sensor.dht11_pin, GPIO_PULLUP_ONLY);
    while (1) {
        
        esp_pm_lock_acquire(s_dht_lock); // 160 MZ forzado
        int result = dht11_read(&sensor, 5); // 5 intentos de conexion
        esp_pm_lock_release(s_dht_lock); // libera lock CPU max

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


static void power_init(void) {
// IMPORTANTE configurar 
// Component config -> Power Management -> Enable CPU frequency scaling y Enable light sleep
// Component config → FreeRTOS → Kernel -> configEXPECTED_IDLE_TIME_BEFORE_SLEEP tiempo de espera antes de entrar en light sleep (ms) 3000 ms

    esp_pm_config_t pm_cfg = { // CPU normalmente a 40 MZ
        .max_freq_mhz = 160,           // CPU a 160 MHz cuando hay trabajo
        .min_freq_mhz = 40,            // baja a 40 MHz cuando esta inactiva
        .light_sleep_enable = true,    // light sleep automatico cuando todas las tareas esperan
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_cfg)); 
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "dht", &s_dht_lock)); // Forza CPU max durante una tarea
    
    ESP_LOGI("POWER", "max_freq=%d min_freq=%d MHz, light_sleep=%d",
             pm_cfg.max_freq_mhz, pm_cfg.min_freq_mhz, pm_cfg.light_sleep_enable ? 1 : 0);

}