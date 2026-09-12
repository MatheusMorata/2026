#include "task_sensores.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "mpu9250.h"
#include "bmp280.h"
#include "dht22.h"
#include "gps_neo6m.h"

extern QueueHandle_t filaTelemetria;
static const char *TAG = "TASK_SENS";

#define MPU9250_ADDR             0x68
#define BMP280_ADDR_PRIMARY      0x76
#define BMP280_ADDR_SECONDARY    0x77
#define DHT22_GPIO_PIN           GPIO_NUM_4
#define GPS_UART_PORT            UART_NUM_1
#define GPS_TX_PIN               17
#define GPS_RX_PIN               16
#define GPS_BAUDRATE             9600

void task_sensores(void *pvParameters) {
    ESP_LOGI(TAG, "Task de Sensores Iniciada");
    sensorsData_t dados = {0};

    esp_err_t mpu_ok = mpu9250_init(MPU9250_ADDR);
    if (mpu_ok != ESP_OK) {
        ESP_LOGW(TAG, "MPU9250 nao inicializado: %s", esp_err_to_name(mpu_ok));
    }

    esp_err_t bmp_ok = bmp280_init(BMP280_ADDR_PRIMARY);
    if (bmp_ok != ESP_OK) {
        bmp_ok = bmp280_init(BMP280_ADDR_SECONDARY);
    }
    if (bmp_ok != ESP_OK) {
        ESP_LOGW(TAG, "BMP280 nao inicializado (0x76/0x77): %s", esp_err_to_name(bmp_ok));
    }

    esp_err_t dht_ok = dht22_init(DHT22_GPIO_PIN);
    if (dht_ok != ESP_OK) {
        ESP_LOGW(TAG, "DHT22 nao inicializado no GPIO %d: %s", DHT22_GPIO_PIN, esp_err_to_name(dht_ok));
    }

    esp_err_t gps_ok = gps_neo6m_init(GPS_UART_PORT, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUDRATE);
    if (gps_ok != ESP_OK) {
        ESP_LOGW(TAG, "GPS NEO6M nao inicializado: %s", esp_err_to_name(gps_ok));
    }

    TickType_t last_tick = xTaskGetTickCount();
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;

    while (1) {
        TickType_t now = xTaskGetTickCount();
        float dt_s = (float)(now - last_tick) * (float)portTICK_PERIOD_MS / 1000.0f;
        if (dt_s <= 0.0f) dt_s = 0.2f;
        last_tick = now;

        dados.seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;

        if (mpu_ok == ESP_OK) {
            mpu9250_data_t mpu_data;
            if (mpu9250_read(&mpu_data) == ESP_OK) {
                mpu9250_estimate_attitude(&mpu_data, dt_s, &roll, &pitch, &yaw);
                dados.roll = roll;
                dados.pitch = pitch;
                dados.yaw = yaw;
            }
        }

        if (bmp_ok == ESP_OK) {
            float t = 0.0f;
            float p = 0.0f;
            float a = 0.0f;
            if (bmp280_read(&t, &p, &a) == ESP_OK) {
                dados.temperatura = t;
                dados.pressao = p;
                dados.altitude = a;
            }
        }

        if (dht_ok == ESP_OK) {
            float dht_t = 0.0f;
            float dht_h = 0.0f;
            if (dht22_read(&dht_t, &dht_h) == ESP_OK) {
                dados.umidade = dht_h;
                if (bmp_ok != ESP_OK) {
                    dados.temperatura = dht_t;
                }
            }
        }

        if (gps_ok == ESP_OK) {
            gps_fix_t fix;
            if (gps_neo6m_poll(&fix) == ESP_OK && fix.valid) {
                dados.latitude = fix.latitude;
                dados.longitude = fix.longitude;
                dados.sats = fix.sats;
            }
        }

        if (xQueueSend(filaTelemetria, &dados, pdMS_TO_TICKS(200)) != pdPASS) {
            ESP_LOGW(TAG, "Fila de telemetria cheia!");
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // 5Hz
    }
}
