#ifndef DHT11_H_
#define DHT11_H_

#include "driver/gpio.h"

enum dht11_status {
    DHT11_CRC_ERROR = -2,
    DHT11_TIMEOUT_ERROR,
    DHT11_OK
};

typedef struct{
    uint8_t status;
    double temperature;
    uint8_t humidity;
}dht_t;

void DHT11_init(gpio_num_t);

dht_t DHT11_read();

#endif //DHT11_H