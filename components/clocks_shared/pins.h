#pragma once

#include "../clocks_shared/stub.h"

#ifdef MASTER

const int MASTER_GPI0_PIN = 0;  // 18; // GPI00
// const int ESP_TXD_PIN = 1;      // 22; // GPI01/TXD
// const int RS485_RE_PIN = 2;  // 17; // GPI02
// const int ESP_RXD_PIN = 3;      // 21; // GPI03/RXD
const int I2C_SDA_PIN = 4;     // 19; // GPI04
const int I2C_SCL_PIN = 5;     // 20; // GPI05
const int SYNC_OUT_PIN = 12;   // 6; // GPI12
const int SYNC_IN_PIN = 13;    // 7; // GPI13
const int GPIO_14 = 14;        // 5; // GPI14
const int RS485_DE_PIN = 15;   // 16; // GPI15
const int USB_POWER_PIN = 16;  // 4; // GPI16

#else

#include <FastGPIO.h>
#define USE_FAST_GPIO_FOR_SYNC__

constexpr uint8_t SYNC_OUT_PIN = 3;  // PD3
constexpr uint8_t SYNC_IN_PIN = 2;   // PD2

constexpr uint8_t MOTOR_SLEEP_PIN = 4;  // PD4
constexpr uint8_t MOTOR_ENABLE = 10;    // PB2
constexpr uint8_t MOTOR_RESET = 9;      // PB1

constexpr uint8_t MOTOR_A_DIR = 5;   // PD5
constexpr uint8_t MOTOR_A_STEP = 6;  // PD6

constexpr uint8_t MOTOR_B_DIR = 7;   // PD7
constexpr uint8_t MOTOR_B_STEP = 8;  // PB0

const uint8_t SLAVE_RS485_RXD_DPIN = 1;  // PD1 RO !RXD
const uint8_t SLAVE_POS_A = 16;          // A2
const uint8_t SLAVE_POS_B = 17;          // A3
const uint8_t RS485_RE_PIN = 18;         // A4 // RE
const uint8_t RS485_DE_PIN = 19;         // A5 // DE

const uint8_t LED_DATA_PIN = 11;
const uint8_t LED_CLOCK_PIN = 13;

const uint8_t SLAVE_RS485_TXD_DPIN = 0;  // PD0 DI !TXD

#endif
