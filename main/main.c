#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/adc.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"
// Heart beat blink period
#define BLINK_PERIOD    1000

// Sensor Period
#define SENSOR_PERIOD   5000
#define SENSOR_N_VALUES 10 
#define LED_PIN     GPIO_NUM_2

// Prototypes
esp_err_t event_handler(void *ctx, system_event_t *event);
void HeartBeat(void *pvParameter);
void SensorReading(void *pvParameter);
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
static void mqtt_app_start(void);
//*****************************************************************************************************

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

//*****************************************************************************************************
// MQTT Cloud INFO
const char MQTT_URI[] = "mqtt://postman.cloudmqtt.com";
const char MQTT_USERNAME[] = "mggphudi";
const char MQTT_PASSWORD[] = "wauG8UCgVkct";
const uint32_t MQTT_PORT = 17936;
static const char *TAG_MQTT = "MQTT_SAMPLE";

//*****************************************************************************************************

void app_main(void)
{
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "Teste1",
            .password = "pelepelezando",
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
    
    ESP_LOGI("MAIN", "Inicio");
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_5,ADC_ATTEN_DB_6);

    mqtt_app_start();

    xTaskCreate(&HeartBeat, "HB", 1024, NULL, 2, NULL);
    xTaskCreate(&SensorReading, "SensorReading", 2048, NULL, 5, NULL);

    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//*****************************************************************************************************

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

//*****************************************************************************************************

void HeartBeat(void *pvParameter)
{
    int level = 0;
    while (true) {
        gpio_set_level(LED_PIN, level);
        level = !level;
        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

//*****************************************************************************************************

void SensorReading(void *pvParameter)
{
    int valueRawList[SENSOR_N_VALUES] = {0};
    uint32_t valueRaw = 0;
    uint8_t idx = 0;
    float valueV = 0;



    while(1)
    {
        // Media movel
        if(idx < SENSOR_N_VALUES)
        {
            valueRawList[idx] = adc1_get_raw(ADC1_CHANNEL_5);
            idx++;
        }else{
            for(uint8_t i = SENSOR_N_VALUES; i>0; i--)
            {
                valueRawList[i] =  valueRawList[i-1];
            }
            valueRawList[0] = adc1_get_raw(ADC1_CHANNEL_5);
        }

        for(uint8_t i = 0; i<SENSOR_N_VALUES; i++)
        {
            valueRaw += valueRawList[i];
        }


        valueRaw = valueRaw/SENSOR_N_VALUES;
        valueV = 2 * (float)valueRaw*3.6/4095;


        ESP_LOGI("SensorReading", "valueRaw = %d\n", valueRaw);
        ESP_LOGI("SensorReading", "valueV = %1.2f V\n", valueV);

        valueRaw = 0;

        vTaskDelay(SENSOR_PERIOD / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}



static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_URI,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .port = MQTT_PORT,
        .event_handle = mqtt_event_handler,
        
        // .user_context = (void *)your_context
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG_MQTT, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
            break;
        default:

        break;
    }
    return ESP_OK;
}