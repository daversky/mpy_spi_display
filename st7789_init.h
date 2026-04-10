#ifndef ST7789_INIT_H
#define ST7789_INIT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t cmd;
    size_t data_len;
    const uint8_t *data;
    uint16_t delay_ms;
} st7789_init_cmd_t;

static const uint8_t st7789_init_data_1[] = {0x00};
static const uint8_t st7789_init_data_2[] = {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C};
static const uint8_t st7789_init_data_3[] = {0x05, 0x14};
static const uint8_t st7789_init_data_4[] = {0x0A, 0x0C};
static const uint8_t st7789_init_data_5[] = {0x0E, 0x0F, 0x08, 0x0A, 0x0D, 0x00};
static const uint8_t st7789_init_data_6[] = {0x00};
static const uint8_t st7789_init_data_7[] = {0x55};  // 16-bit/pixel
static const uint8_t st7789_init_data_8[] = {0x00, 0x00, 0x00, 0xF0};
static const uint8_t st7789_init_data_9[] = {0x00, 0x00, 0x00, 0xF0};

static const st7789_init_cmd_t st7789_init_sequence[] = {
    {0x01, 0, NULL, 150},              // SWRESET
    {0x11, 0, NULL, 50},               // SLPOUT
    {0x36, 1, st7789_init_data_1, 0},  // MADCTL
    {0x3A, 1, st7789_init_data_7, 0},  // COLMOD (16-bit/pixel)
    {0xB2, 6, st7789_init_data_2, 0},  // PORCTRL
    {0xB7, 1, st7789_init_data_6, 0},  // GCTRL
    {0xBB, 1, st7789_init_data_1, 0},  // VCOMS
    {0xC0, 1, st7789_init_data_4, 0},  // LCMCTRL
    {0xC2, 2, st7789_init_data_3, 0},  // VDVVRHEN
    {0xC3, 2, st7789_init_data_4, 0},  // VRHS
    {0xC4, 2, st7789_init_data_4, 0},  // VDVS
    {0xC6, 1, st7789_init_data_1, 0},  // FRCTRL2
    {0xD0, 2, st7789_init_data_5, 0},  // PWCTRL1
    {0xE0, 14, st7789_init_data_8, 0}, // PVGAMCTRL
    {0xE1, 14, st7789_init_data_9, 0}, // NVGAMCTRL
    {0x21, 0, NULL, 0},                // INVON
    {0x29, 0, NULL, 100},              // DISPON
    {0x2C, 0, NULL, 0},                // RAMWR
};

#define ST7789_INIT_SEQUENCE_LEN (sizeof(st7789_init_sequence) / sizeof(st7789_init_cmd_t))

#endif // ST7789_INIT_H