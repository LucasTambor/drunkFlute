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

#include "DHT11.h"

// Heart beat blink period
#define BLINK_PERIOD    1000

// MQ3 Sensor Period
#define SENSOR_PERIOD   60000
#define SENSOR_N_VALUES 10 
#define LED_PIN     GPIO_NUM_2

// DHT
#define DHT_PIN     32
#define READ_DHT_PERIOD 60000
// Prototypes
esp_err_t wifi_event_handler(void *ctx, system_event_t *event);
static void wifi_init(void);
void HeartBeat(void *pvParameter);
void SensorReading(void *pvParameter);
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
static esp_mqtt_client_handle_t mqtt_app_start(void);
void dhtRead(void *pvParameter);
void sendMqtt();
//*****************************************************************************************************

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;
dht_t dht_value;

//*****************************************************************************************************
// MQTT Cloud INFO
const char MQTT_URI[] = "mqtt://io.adafruit.com";
const char MQTT_USERNAME[] = "Tambor";
const char MQTT_PASSWORD[] = "be9dba72f78445fbb91d63d7b1ef4989";
const uint32_t MQTT_PORT = 1883;
static const char *TAG_MQTT = "MQTT_SAMPLE";

const char temp_topic[] = "Tambor/f/temperatura";
const char hum_topic[] = "Tambor/f/umidade";
const char mq3_topic[] = "Tambor/f/mq3";

esp_mqtt_client_handle_t globalClient;

// SemaphoreHandle_t xSemaphore_mq3 = NULL;
// SemaphoreHandle_t xSemaphore_dht = NULL;
//*****************************************************************************************************

void app_main(void)
{
    nvs_flash_init();
    wifi_init();

    // MQTT Config
    globalClient = mqtt_app_start();
    
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // ADC Config
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_5,ADC_ATTEN_DB_6);


    // Task creation
    xTaskCreate(&HeartBeat, "HB", 1024, NULL, 2, NULL);
    xTaskCreate(&SensorReading, "SensorReading", 2048, NULL, 3, NULL);
    xTaskCreate(&dhtRead, "dhtRead", 2048, NULL, 5, NULL);
    // xTaskCreate(&sendMqtt, "sendMqtt", 1024, NULL, 4, NULL);

    // Mutex creation
    // xSemaphore_dht = xSemaphoreCreateMutex();
    // xSemaphore_mq3 = xSemaphoreCreateMutex();

    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//*****************************************************************************************************

esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
   switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

//*****************************************************************************************************
static void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
   wifi_config_t sta_config = {
        .sta = {
            .ssid = "Teste1",
            .password = "pelepelezando",
            .bssid_set = false
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
    ESP_LOGI("wifi_init", "start the WIFI SSID:[%s]", "Teste1");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI("wifi_init", "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
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

    char buf[10];

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
        
        sprintf(buf, "%2.2f", valueV);
        esp_mqtt_client_publish(globalClient, mq3_topic, buf, 0, 0, 0);



        ESP_LOGI("SensorReading", "valueRaw = %d\n", valueRaw);
        ESP_LOGI("SensorReading", "valueV = %1.2f V\n", valueV);

        valueRaw = 0;

        vTaskDelay(SENSOR_PERIOD / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

//*****************************************************************************************************

static esp_mqtt_client_handle_t mqtt_app_start(void)
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

    return client;
}

//*****************************************************************************************************

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
            // msg_id = esp_mqtt_client_subscribe(client, "Tambor/f/qos0", 0);
        break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            // msg_id = esp_mqtt_client_publish(client, "Tambor/f/qos0", "20", 0, 0, 0);
            // ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d", msg_id);
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

//*****************************************************************************************************

void dhtRead(void *pvParameter)
{
    ESP_LOGI("DHT", "Initializing DHT READ");
    DHT11_init(DHT_PIN);
    dht_t actual_read_dht;
    char buf[10];

    while(1)
    {
        actual_read_dht = DHT11_read();

        if(actual_read_dht.status == DHT11_OK)
        {
            dht_value = actual_read_dht;

            

            ESP_LOGI("DHT", "READ SUCESSFULL");
            ESP_LOGI("DHT", "Temp = %2.2f, Hum = %d\n", dht_value.temperature, dht_value.humidity);
        }else{
            ESP_LOGI("DHT", "READ ERROR = %d\n", dht_value.status);
        }

        sprintf(buf, "%2.2f", dht_value.temperature);
        esp_mqtt_client_publish(globalClient, temp_topic, buf, 0, 0, 0);
        sprintf(buf, "%d", dht_value.humidity);
        esp_mqtt_client_publish(globalClient, hum_topic, buf, 0, 0, 0);

        vTaskDelay(READ_DHT_PERIOD/portTICK_PERIOD_MS);
    }

}