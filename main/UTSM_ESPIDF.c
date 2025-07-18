#include <inttypes.h>              // For PRIx32 (cross-platform safe integer printing)
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "TWAI"

// ---------------- SEND TASK ----------------
void send_task(void *arg) {
    while (1) {
        twai_message_t message = {
            .identifier = 0x123,
            .data_length_code = 8,
            .data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}
        };

        esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(1000));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Message sent successfully");
        } else {
            ESP_LOGE(TAG, "Message transmission failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- RECEIVE TASK ----------------
void receive_task(void *arg) {
    while (1) {
        twai_message_t message;
        esp_err_t err = twai_receive(&message, pdMS_TO_TICKS(1000));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Message received: ID=0x%" PRIx32 ", Data=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                     message.identifier,
                     message.data[0], message.data[1], message.data[2], message.data[3],
                     message.data[4], message.data[5], message.data[6], message.data[7]);
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Reception timed out");
        } else {
            ESP_LOGE(TAG, "Message reception failed: %s", esp_err_to_name(err));
        }
    }
}

// ---------------- MAIN ----------------
void app_main(void) {
    esp_log_level_set("*", ESP_LOG_INFO);  // Ensure all INFO logs are visible

    // Configure TWAI (CAN) peripheral
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_32, GPIO_NUM_33, TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "TWAI driver installed successfully");

    // Start TWAI driver
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "TWAI driver started successfully");

    // Create send and receive tasks
    BaseType_t tx_created = xTaskCreate(send_task, "send_task", 2048, NULL, 5, NULL);
    BaseType_t rx_created = xTaskCreate(receive_task, "receive_task", 2048, NULL, 5, NULL);
    
    if (tx_created != pdPASS || rx_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create one or more tasks");
    }
}
