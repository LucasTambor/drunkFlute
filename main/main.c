#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "pcd8544.h"
#include "esp_log.h"


#define BLINK_PERIOD    1000

// Event group
static EventGroupHandle_t event_group;
QueueHandle_t buffer;

// Prototypes
esp_err_t event_handler(void *ctx, system_event_t *event);
void heart_beat(void *pvParameter);
void sensorReading(void *pvParameter);
void lcdControl(void *pvParameter);


void app_main(void)
{
    // nvs_flash_init();
    // tcpip_adapter_init();
    // ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    // ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    // ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    // wifi_config_t sta_config = {
    //     .sta = {
    //         .ssid = "Teste1",
    //         .password = "pelepelezando",
    //         .bssid_set = false
    //     }
    // };
    // ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    // ESP_ERROR_CHECK( esp_wifi_start() );
    // ESP_ERROR_CHECK( esp_wifi_connect() );
    buffer = xQueueCreate(1, 4);
    xTaskCreate(&heart_beat, "HB", 2048, NULL, 5, NULL);
    xTaskCreate(&lcdControl, "lcd", 2048, NULL, 5, NULL);

    while(1)
    {

    }
}

void lcdControl(void *pvParameter)
{
    uint32_t value;
    char bufStr[32];
    pcd8544_config_t config = {
        .spi_host = HSPI_HOST,
        .is_backlight_common_anode = true,
    };
    pcd8544_init(&config);

    while(1)
    {
        xQueueReceive(buffer, &value ,portMAX_DELAY);
        ESP_LOGI("LCD", "Valor do contador recebido : %d", value);
        sprintf(bufStr, "Cont: %d", value);
        pcd8544_clear_display();
        pcd8544_finalize_frame_buf();
        pcd8544_set_pos(0, 0);
        pcd8544_puts(bufStr);
        pcd8544_sync_and_gc();
    }

    vTaskDelete(NULL);
}

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}


void heart_beat(void *pvParameter)
{
    static uint32_t counter = 0;
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    int level = 0;
    while (true) {
        counter++;
        gpio_set_level(GPIO_NUM_2, level);
        level = !level;
        
        xQueueSend(buffer, &counter, pdMS_TO_TICKS(0));

        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void sensorReading(void *pvParameter)
{

}

