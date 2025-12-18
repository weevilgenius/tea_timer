#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"

static const char *TAG = "tea_timer";

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "Boot OK");
    ESP_LOGI(TAG, "ESP32-S3 cores=%d, revision=%d",
             chip_info.cores, chip_info.revision);

    uint32_t flash_size = 0;
    if (esp_flash_get_physical_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "Flash size: %lu MB",
                 (unsigned long)(flash_size / (1024 * 1024)));
    } else {
        ESP_LOGW(TAG, "Flash size query failed");
    }

    for (int i = 0;; i++) {
        ESP_LOGI(TAG, "tick %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}