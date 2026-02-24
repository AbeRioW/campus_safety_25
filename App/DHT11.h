#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"
#include "gpio.h"
#include "stdio.h"
#include "stdbool.h"


#define DHT11_PIN_SET   HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_SET)                                            //  设置GPIO为高
#define DHT11_PIN_RESET HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_RESET)                                          //  设置GPIO为低
#define DHT11_READ_IO   HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_4)                                                          //  DHT11 GPIO定义

typedef struct {
    uint8_t humidity_int;    // 湿度整数部分
    uint8_t humidity_dec;    // 湿度小数部分（DHT11始终为0）
    uint8_t temp_int;        // 温度整数部分
    uint8_t temp_dec;        // 温度小数部分（DHT11始终为0）
    uint8_t checksum;        // 校验和
} DHT11_Data_t;

void DHT11_START(void);
unsigned char DHT11_READ_BIT(void);
unsigned char DHT11_READ_BYTE(void);
unsigned char DHT11_READ_DATA(DHT11_Data_t *dht_data);
unsigned char DHT11_Check(void);
static void DHT11_GPIO_MODE_SET(uint8_t mode);

void Coarse_delay_us(uint32_t us);

void lay_control(bool open);
#endif