// spi_displays/st7789.c
#include "display.h"
#include "py/mphal.h"


static const display_init_cmd_t st7789_init_sequence[] = {
    {0x01, 0, 150, {}}, // SWRESET: Программный сброс
    {0x11, 0, 150, {}}, // SLPOUT: Выход из спящего режима
    {0x3A, 1, 0, {0x55}}, // COLMOD: 16-bit color (0x55 или 0x05)
    {0x36, 1, 0, {0x00}}, // MADCTL: Ориентация (0x00 - default)
    {0xB2, 5, 0, {0x0C, 0x0C, 0x00, 0x33, 0x33}}, // PORCTRL
    {0xB7, 1, 0, {0x35}}, // GATECTRL
    {0xBB, 1, 0, {0x19}}, // VCOMS
    {0xC0, 1, 0, {0x2C}}, // LCMCTRL
    {0xC2, 1, 0, {0x01}}, // VDVVRHEN
    {0xC3, 1, 0, {0x12}}, // VRHS
    {0xC4, 1, 0, {0x20}}, // VDVS
    {0xD0, 2, 0, {0xA4, 0xA1}}, // POWERCTRL1
    {0xE0, 14, 0, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}},
    {0xE1, 14, 0, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}},
    {0x21, 0, 0, {}}, // INVON: Включить инверсию (для IPS матриц)
    {0x29, 0, 100, {}}, // DISPON: Включить дисплей
    {0, 0, 0, {}} // Маркер конца списка
};

static uint8_t st7789_get_madctl_value(mp_display_obj_t* self) {
    uint8_t madctl = self->bgr ? 0x08 : 0x00;
    switch (self->rotate) {
        case 0: madctl |= 0x00; break;
        case 1: madctl |= 0x60; break;
        case 2: madctl |= 0xC0; break;
        case 3: madctl |= 0xA0; break;
    }
    return madctl;
}

static void st7789_init(mp_display_obj_t* self) {
    if (self->rst != (mp_hal_pin_obj_t)-1) {
        display_reset_hw(self);
    }
    for (const display_init_cmd_t* entry = st7789_init_sequence; entry->cmd != 0x00; entry++) {
        uint8_t cmd = entry->cmd;
        const uint8_t* data = entry->data;
        uint8_t len = entry->len;

        uint8_t dynamic_data;
        if (cmd == 0x36) {
            dynamic_data = st7789_get_madctl_value(self);
            data = &dynamic_data;
            len = 1;
        }
        display_write_cmd_data(self, cmd, data, len);
        if (entry->delay > 0) {
            mp_hal_delay_ms(entry->delay);
        }
    }
    display_write_cmd_data(self, self->inverse ? 0x21 : 0x20, NULL, 0);
    display_set_window(self, 0, 0, self->width, self->height);
}

static mp_obj_t st7789_make_new(const mp_obj_type_t* type, size_t n_args, size_t n_kw, const mp_obj_t* all_args) {
    // Создаем DisplayCore объект
    mp_obj_t display_obj = display_make_new_base(&mp_type_display_core, n_args, n_kw, all_args);
    mp_display_obj_t* self = MP_OBJ_TO_PTR(display_obj);
    st7789_init(self);
    self->base.type = type;
    return display_obj;
}

static const mp_rom_map_elem_t st7789_locals_dict_table[] = {};
static MP_DEFINE_CONST_DICT(st7789_locals_dict, st7789_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_st7789,
    MP_QSTR_ST7789,
    MP_TYPE_FLAG_NONE,
    make_new, st7789_make_new,
    locals_dict, &st7789_locals_dict,
    attr, display_attr,
    parent, &mp_type_display
);