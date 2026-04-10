#ifndef ST7735_INIT_H
#define ST7735_INIT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t cmd;
    size_t data_len;
    const uint8_t *data;
    uint16_t delay_ms;
} st7735_init_cmd_t;

static const uint8_t st7735_init_data_1[] = {0x01, 0x2C, 0x2D};
static const uint8_t st7735_init_data_2[] = {0x01, 0x2C, 0x2D};
static const uint8_t st7735_init_data_3[] = {0xA2, 0xA0, 0xA8, 0x2B, 0x2A};
static const uint8_t st7735_init_data_4[] = {0x01};
static const uint8_t st7735_init_data_5[] = {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C};
static const uint8_t st7735_init_data_6[] = {0x05, 0x14};
static const uint8_t st7735_init_data_7[] = {0x0E, 0x0F, 0x08, 0x0A, 0x0D, 0x00};
static const uint8_t st7735_init_data_8[] = {0x2C};
static const uint8_t st7735_init_data_9[] = {0x0A, 0x0C};
static const uint8_t st7735_init_data_10[] = {0x3C, 0x2D, 0x36, 0x3F, 0x3C};
static const uint8_t st7735_init_data_11[] = {0x32, 0x3F, 0x3F};
static const uint8_t st7735_init_data_12[] = {0x00};
static const uint8_t st7735_init_data_13[] = {0x00};
static const uint8_t st7735_init_data_14[] = {0x00, 0x17, 0x15};
static const uint8_t st7735_init_data_15[] = {0x00, 0x02, 0x1F};

static const st7735_init_cmd_t st7735_init_sequence[] = {
    {0x01, 0, NULL, 150},              // SWRESET
    {0x11, 0, NULL, 50},               // SLPOUT
    {0xB1, 3, st7735_init_data_1, 0},  // FRMCTR1
    {0xB2, 3, st7735_init_data_2, 0},  // FRMCTR2
    {0xB3, 6, st7735_init_data_3, 0},  // FRMCTR3
    {0xB4, 1, st7735_init_data_4, 0},  // INVCTR
    {0xC0, 6, st7735_init_data_5, 0},  // PWCTR1
    {0xC1, 2, st7735_init_data_6, 0},  // PWCTR2
    {0xC2, 2, st7735_init_data_9, 0},  // PWCTR3
    {0xC3, 2, st7735_init_data_9, 0},  // PWCTR4
    {0xC4, 2, st7735_init_data_9, 0},  // PWCTR5
    {0xC5, 2, st7735_init_data_7, 0},  // VMCTR1
    {0x36, 1, st7735_init_data_8, 0},  // MADCTL
    {0x3A, 1, st7735_init_data_11, 0}, // COLMOD (16-bit/pixel)
    {0xE0, 6, st7735_init_data_8, 0},  // GMCTRP1
    {0xE1, 6, st7735_init_data_9, 0},  // GMCTRN1
    {0x2A, 4, st7735_init_data_10, 0}, // CASET
    {0x2B, 4, st7735_init_data_11, 0}, // RASET
    {0x13, 0, NULL, 0},                // NORON
    {0x29, 0, NULL, 100},              // DISPON
    {0x2C, 0, NULL, 0},                // RAMWR
};

#define ST7735_INIT_SEQUENCE_LEN (sizeof(st7735_init_sequence) / sizeof(st7735_init_cmd_t))

#endif // ST7735_INIT_H