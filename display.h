#ifndef MICROPYTHON_DISPLAY_H
#define MICROPYTHON_DISPLAY_H

#include "py/obj.h"
#include "py/runtime.h"
#include "extmod/machine_spi.h"

typedef struct _display_obj_t {
    mp_obj_base_t base;

    // SPI
    mp_obj_base_t *spi_obj;
    uint32_t baudrate;

    // Pins
    int8_t pin_dc;
    int8_t pin_cs;
    int8_t pin_rst;
    int8_t pin_bl;

    // Display dimensions
    uint16_t width;
    uint16_t height;

    // Backlight settings
    bool backlight_active_high;
    bool pwm_available;

    // DMA settings
    bool use_dma;
    size_t dma_threshold;

    // Internal state
    bool cs_active_high;  // Usually false for CS

    // Platform-specific DMA handle (opaque)
    void *dma_handle;
} display_obj_t;

// Public API
mp_obj_t display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);
void display_cmd(display_obj_t *self, uint8_t cmd);
void display_cmd_data(display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t len);
void display_show(display_obj_t *self, const uint8_t *buffer, size_t len);
void display_update_rect(display_obj_t *self, uint16_t x, uint16_t y,
                         uint16_t w, uint16_t h, const uint8_t *buffer, size_t len);
void display_set_backlight(display_obj_t *self, uint8_t percent);

extern const mp_obj_type_t display_type;

#endif // MICROPYTHON_DISPLAY_H