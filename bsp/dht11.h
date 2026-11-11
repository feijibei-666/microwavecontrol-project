#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

typedef enum
{
    DHT11_WAITING,
    DHT11_OK,
    DHT11_TIMEOUT,
    DHT11_TIMING,
    DHT11_CHECKSUM
} Dht11Status;

typedef struct
{
    uint8_t temperature_c;
    uint8_t humidity_percent;
    uint8_t status;
} Dht11Reading;

void Dht11_Init(void);
Dht11Reading Dht11_Read(void);

#endif
