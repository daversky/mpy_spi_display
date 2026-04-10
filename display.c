#include "display.h"
#include "display_dma.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "extmod/machine_spi.h"

// Platform-specific GPIO includes
#if defined(ESP32)
    #include "driver/gpio.h"
#elif defined(PICO_PLATFORM)
    #include "hardware/gpio.h"
    #include "hardware/pwm.h"
#endif

// Helper macros for pin control
#if defined(ESP32)
    #define PIN_LOW(pin) gpio_set_level(pin, 0)
    #define PIN_HIGH(pin) gpio_set_level(pin, 1)
    #define PIN_OUTPUT(pin) gpio_set_direction(pin, GPIO_MODE_OUTPUT)
#elif defined(PICO_PLATFORM)
    #define PIN_LOW(pin) gpio_put(pin, 0)
    #define PIN_HIGH(pin) gpio_put(pin, 1)
    #define PIN_OUTPUT(pin) gpio_init(pin); gpio_set_dir(pin, GPIO_OUT)
#else
    #define PIN_LOW(pin) mp_hal_pin_low(pin)
    #define PIN_HIGH(pin) mp_hal_pin_high(pin)
    #define PIN_OUTPUT(pin) mp_hal_pin_output(pin)
#endif

static inline void _dc_low(display_obj_t *self) {
    if (self->pin_dc >= 0) PIN_LOW(self->pin_dc);
}

static inline void _dc_high(display_obj_t *self) {
    if (self->pin_dc >= 0) PIN_HIGH(self->pin_dc);
}

static inline void _cs_low(display_obj_t *self) {
    if (self->pin_cs >= 0) {
        if (self->cs_active_high) {
            PIN_HIGH(self->pin_cs);
        } else {
            PIN_LOW(self->pin_cs);
        }
    }
}

static inline void _cs_high(display_obj_t *self) {
    if (self->pin_cs >= 0) {
        if (self->cs_active_high) {
            PIN_LOW(self->pin_cs);
        } else {
            PIN_HIGH(self->pin_cs);
        }
    }
}

static void _spi_write(display_obj_t *self, const uint8_t *data, size_t len) {
    machine_spi_obj_t *spi = (machine_spi_obj_t *)self->spi_obj;

    if (self->use_dma && self->dma_handle && len >= self->dma_threshold) {
        // Use DMA for large transfers
        size_t chunk_size = display_dma_chunk_size(self->dma_handle);
        const uint8_t *ptr = data;
        size_t remaining = len;

        while (remaining > 0) {
            size_t chunk = (remaining > chunk_size) ? chunk_size : remaining;
            size_t sent = display_dma_send(self->dma_handle, ptr, chunk);
            if (sent == 0) {
                // Fallback to normal SPI
                mp_spi_transfer(spi, chunk, ptr, NULL, SPI_XFER_BLOCKING);
            }
            ptr += chunk;
            remaining -= chunk;
        }
    } else {
        // Normal SPI transfer
        mp_spi_transfer(spi, len, data, NULL, SPI_XFER_BLOCKING);
    }
}

static void _set_window(display_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x2 = x + w - 1;
    uint16_t y2 = y + h - 1;

    // Column address set
    _dc_low(self);
    uint8_t caset_data[] = {
        (uint8_t)(x >> 8), (uint8_t)(x & 0xFF),
        (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF)
    };
    _spi_write(self, (uint8_t[]){0x2A}, 1);
    _dc_high(self);
    _spi_write(self, caset_data, 4);

    // Row address set
    _dc_low(self);
    uint8_t raset_data[] = {
        (uint8_t)(y >> 8), (uint8_t)(y & 0xFF),
        (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF)
    };
    _spi_write(self, (uint8_t[]){0x2B}, 1);
    _dc_high(self);
    _spi_write(self, raset_data, 4);

    // Memory write
    _dc_low(self);
    _spi_write(self, (uint8_t[]){0x2C}, 1);
    _dc_high(self);
}

void display_cmd(display_obj_t *self, uint8_t cmd) {
    _cs_low(self);
    _dc_low(self);
    _spi_write(self, &cmd, 1);
    _cs_high(self);
}

void display_cmd_data(display_obj_t *self, uint8_t cmd, const uint8_t *data, size_t len) {
    _cs_low(self);

    _dc_low(self);
    _spi_write(self, &cmd, 1);

    if (len > 0) {
        _dc_high(self);
        _spi_write(self, data, len);
    }

    _cs_high(self);
}

void display_show(display_obj_t *self, const uint8_t *buffer, size_t len) {
    size_t expected_len = (size_t)self->width * self->height * 2;
    if (len != expected_len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer size mismatch"));
    }

    _set_window(self, 0, 0, self->width, self->height);
    _cs_low(self);
    _dc_high(self);

    // Send in chunks for large displays
    if (len > 128 * 1024) {
        size_t chunk_size = 16 * self->width * 2;
        const uint8_t *ptr = buffer;
        size_t remaining = len;

        while (remaining > 0) {
            size_t chunk = (remaining > chunk_size) ? chunk_size : remaining;
            _spi_write(self, ptr, chunk);
            ptr += chunk;
            remaining -= chunk;
        }
    } else {
        _spi_write(self, buffer, len);
    }

    _cs_high(self);
}

void display_update_rect(display_obj_t *self, uint16_t x, uint16_t y,
                         uint16_t w, uint16_t h, const uint8_t *buffer, size_t len) {
    // Boundary checking and clipping
    if (x >= self->width || y >= self->height) {
        mp_warning("update_rect: coordinates out of bounds");
        return;
    }

    if (x + w > self->width) {
        w = self->width - x;
        mp_warning("update_rect: width clipped to visible area");
    }

    if (y + h > self->height) {
        h = self->height - y;
        mp_warning("update_rect: height clipped to visible area");
    }

    if (w == 0 || h == 0) {
        return;
    }

    // Check buffer size
    size_t expected_len = (size_t)w * h * 2;
    if (len != expected_len) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer size mismatch"));
    }

    _set_window(self, x, y, w, h);
    _cs_low(self);
    _dc_high(self);
    _spi_write(self, buffer, len);
    _cs_high(self);
}

void display_set_backlight(display_obj_t *self, uint8_t percent) {
    if (self->pin_bl < 0) {
        return;
    }

    if (percent > 100) {
        percent = 100;
    }

    if (!self->pwm_available) {
        // GPIO only mode
        if (self->backlight_active_high) {
            percent > 0 ? PIN_HIGH(self->pin_bl) : PIN_LOW(self->pin_bl);
        } else {
            percent > 0 ? PIN_LOW(self->pin_bl) : PIN_HIGH(self->pin_bl);
        }
        return;
    }

    #if defined(ESP32)
        // ESP32 PWM using LEDC
        // Implementation depends on ESP-IDF version
        // Simplified: use GPIO for now
        if (self->backlight_active_high) {
            percent > 50 ? PIN_HIGH(self->pin_bl) : PIN_LOW(self->pin_bl);
        } else {
            percent > 50 ? PIN_LOW(self->pin_bl) : PIN_HIGH(self->pin_bl);
        }
    #elif defined(PICO_PLATFORM)
        // RP2040 PWM
        uint slice_num = pwm_gpio_to_slice_num(self->pin_bl);
        uint channel = pwm_gpio_to_channel(self->pin_bl);

        // Set frequency to 1kHz
        uint32_t clock = 125000000;
        uint32_t divider = 100;
        uint32_t wrap = clock / divider / 1000;

        pwm_set_clkdiv(slice_num, divider);
        pwm_set_wrap(slice_num, wrap - 1);

        // Set duty cycle
        uint16_t level = (uint16_t)((uint32_t)percent * wrap / 100);
        pwm_set_chan_level(slice_num, channel, level);

        pwm_set_enabled(slice_num, true);
    #else
        // Fallback
        if (self->backlight_active_high) {
            percent > 50 ? PIN_HIGH(self->pin_bl) : PIN_LOW(self->pin_bl);
        } else {
            percent > 50 ? PIN_LOW(self->pin_bl) : PIN_HIGH(self->pin_bl);
        }
    #endif
}

// Python bindings
static mp_obj_t display_make_new_c(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_spi, ARG_width, ARG_height, ARG_dc, ARG_cs, ARG_rst, ARG_bl,
        ARG_backlight_active_high, ARG_use_dma, ARG_dma_threshold
    };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_cs, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_rst, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_bl, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_backlight_active_high, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_use_dma, MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_dma_threshold, MP_ARG_INT, {.u_int = 64} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Validate arguments
    int width = args[ARG_width].u_int;
    int height = args[ARG_height].u_int;
    if (width <= 0 || height <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid dimensions"));
    }

    // Get SPI object
    mp_obj_t spi_obj = args[ARG_spi].u_obj;
    if (!mp_obj_is_type(spi_obj, &machine_spi_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("SPI object expected"));
    }

    // Create display object
    display_obj_t *self = m_new_obj(display_obj_t);
    self->base.type = &display_type;

    self->spi_obj = MP_OBJ_TO_PTR(spi_obj);
    self->width = width;
    self->height = height;
    self->pin_dc = args[ARG_dc].u_int;
    self->pin_cs = args[ARG_cs].u_int;
    self->pin_rst = args[ARG_rst].u_int;
    self->pin_bl = args[ARG_bl].u_int;
    self->backlight_active_high = args[ARG_backlight_active_high].u_bool;
    self->use_dma = args[ARG_use_dma].u_bool;
    self->dma_threshold = args[ARG_dma_threshold].u_int;
    self->cs_active_high = false;
    self->dma_handle = NULL;
    self->pwm_available = false;

    // Initialize pins
    if (self->pin_dc >= 0) PIN_OUTPUT(self->pin_dc);
    if (self->pin_cs >= 0) PIN_OUTPUT(self->pin_cs);
    if (self->pin_rst >= 0) PIN_OUTPUT(self->pin_rst);
    if (self->pin_bl >= 0) PIN_OUTPUT(self->pin_bl);

    // Hardware reset
    if (self->pin_rst >= 0) {
        PIN_HIGH(self->pin_rst);
        mp_hal_delay_ms(5);
        PIN_LOW(self->pin_rst);
        mp_hal_delay_ms(20);
        PIN_HIGH(self->pin_rst);
        mp_hal_delay_ms(150);
    }

    // Initialize backlight PWM if available
    #if defined(PICO_PLATFORM)
        if (self->pin_bl >= 0) {
            gpio_set_function(self->pin_bl, GPIO_FUNC_PWM);
            self->pwm_available = true;
        }
    #endif

    // Initialize DMA if requested and available
    if (self->use_dma && display_dma_available(self->spi_obj)) {
        self->use_dma = display_dma_init(self->spi_obj, &self->dma_handle);
    } else {
        self->use_dma = false;
    }

    // Set initial backlight
    if (self->pin_bl >= 0) {
        display_set_backlight(self, 0);
    }

    // Deselect CS
    _cs_high(self);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t display_cmd_obj(mp_obj_t self_in, mp_obj_t cmd_in) {
    display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t cmd = mp_obj_get_int(cmd_in);
    display_cmd(self, cmd);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_cmd_obj_obj, display_cmd_obj);

static mp_obj_t display_cmd_data_obj(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t data_in) {
    display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t cmd = mp_obj_get_int(cmd_in);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    display_cmd_data(self, cmd, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(display_cmd_data_obj_obj, display_cmd_data_obj);

static mp_obj_t display_show_obj(mp_obj_t self_in, mp_obj_t buffer_in) {
    display_obj_t *self = MP_OBJ_TO_PTR(self_in);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buffer_in, &bufinfo, MP_BUFFER_READ);

    display_show(self, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_show_obj_obj, display_show_obj);

static mp_obj_t display_update_rect_obj(size_t n_args, const mp_obj_t *args) {
    display_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    uint16_t w = mp_obj_get_int(args[3]);
    uint16_t h = mp_obj_get_int(args[4]);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[5], &bufinfo, MP_BUFFER_READ);

    display_update_rect(self, x, y, w, h, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(display_update_rect_obj_obj, 6, 6, display_update_rect_obj);

static mp_obj_t display_set_backlight_obj(mp_obj_t self_in, mp_obj_t percent_in) {
    display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t percent = mp_obj_get_int(percent_in);
    display_set_backlight(self, percent);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(display_set_backlight_obj_obj, display_set_backlight_obj);

static const mp_rom_map_elem_t display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_cmd), MP_ROM_PTR(&display_cmd_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_cmd_data), MP_ROM_PTR(&display_cmd_data_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&display_show_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_rect), MP_ROM_PTR(&display_update_rect_obj_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_backlight), MP_ROM_PTR(&display_set_backlight_obj_obj) },
};
static MP_DEFINE_CONST_DICT(display_locals_dict, display_locals_dict_table);

const mp_obj_type_t display_type = {
    { &mp_type_type },
    .name = MP_QSTR_Display,
    .make_new = display_make_new_c,
    .locals_dict = (mp_obj_dict_t *)&display_locals_dict,
};