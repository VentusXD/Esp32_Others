#include "dht11.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT11";

int wait_for_state(dht11_t *dht11, int state, int timeout)
{
    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(dht11->dht11_pin) != state) { 
        if ((esp_timer_get_time() - start_time) >= timeout) {
            return -1; // Timeout
        }
    }
    return (int)(esp_timer_get_time() - start_time);
}

int dht11_read(dht11_t *dht11, int connection_timeout)
{
        uint8_t received_data[5] = 
    {
        0x00, // Humidity integer part
        0x00, // Humidity decimal part
        0x00, // Temperature integer part
        0x00, // Temperature decimal part
        0x00 // Checksum
    };
    int attempts = 0;

    while (attempts < connection_timeout) {
        attempts++;

        // 1. Enviar senal de inicio (START): LOW durante 20 ms
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(dht11->dht11_pin, 0);
        ets_delay_us(20000); // 20 ms

        // 2. Liberar el bus pasando a modo ENTRADA
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_INPUT);
        ets_delay_us(30);

        // Deshabilitar conmutacion de tareas en FreeRTOS 
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);

        // 3. Handshake del DHT11
        if (wait_for_state(dht11, 0, 100) == -1) {
            portEXIT_CRITICAL(&mux);
            ESP_LOGE(TAG, "Fallo Fase 1 (El sensor no respondio LOW)");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (wait_for_state(dht11, 1, 100) == -1) {
            portEXIT_CRITICAL(&mux);
            ESP_LOGE(TAG, "Fallo Fase 2 (El sensor no paso a HIGH)");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (wait_for_state(dht11, 0, 100) == -1) {
            portEXIT_CRITICAL(&mux);
            ESP_LOGE(TAG, "Fallo Fase 3 (El sensor no bajo para datos)");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 4. Lectura de los 40 bits de datos (5 bytes)
        bool read_error = false;
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 8; j++) {
                if (wait_for_state(dht11, 1, 70) == -1) { // 1 hight a 70 us a recibir 
                    read_error = true;
                    break;
                }

                int high_duration = wait_for_state(dht11, 0, 90); // 0 low a 90 us a recibir
                if (high_duration == -1) {
                    read_error = true;
                    break;
                }

                if (high_duration > 40) {  // 40 us es el umbral para diferenciar entre 0 y 1 (1)
                    received_data[i] |= (1 << (7 - j));  // en j=0 , 1<<7 =>  10000000b
                }
            }
            if (read_error) break;
        }

        portEXIT_CRITICAL(&mux);

        if (read_error) {
            ESP_LOGE(TAG, "Error de tiempo al leer bits");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 5. Verificacion de Checksum
        uint8_t checksum = received_data[0] + received_data[1] + received_data[2] + received_data[3];
        if (checksum == received_data[4]) {
            dht11->humidity = (float)received_data[0] + ((float)received_data[1] / 10.0f);
            dht11->temperature = (float)received_data[2] + ((float)received_data[3] / 10.0f);
            return 0;
        } else {
            ESP_LOGE(TAG, "Error checksum: Calc=0x%02X, Rec=0x%02X", checksum, received_data[4]);
            return -1;
        }
    }

    return -1;
}