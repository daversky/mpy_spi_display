#include "display.h"
#include "py/mphal.h"

// st7735 initialization sequence
typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t delay;
    uint8_t data[16];
} st7735_init_cmd_t;

// cmd, len data, latency, data
static const st7735_init_cmd_t st7735_init_sequence[] = {
    {0x01, 0, 150, {}},
    {0x11, 0, 150, {}},
    {0xB1, 3, 0, {0x01, 0x2C, 0x2D}},
    {0xB2, 3, 0, {0x01, 0x2C, 0x2D}},
    {0xB3, 6, 0, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}},
    {0xB4, 1, 0, {0x07}},
    {0xC0, 3, 0, {0xA2, 0x02, 0x84}},
    {0xC1, 1, 0, {0xC5}},
    {0xC2, 2, 0, {0x0A, 0x00}},
    {0xC3, 2, 0, {0x8A, 0x2A}},
    {0xC4, 2, 0, {0x8A, 0xEE}},
    {0xC5, 1, 0, {0x0E}},
    {0xE0, 16, 0, {0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22, 0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10}},
    {0xE1, 16, 0, {0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E, 0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10}},
    {0x2A, 4, 0, {0x00, 0x00, 0x00, 0x7F}},
    {0x2B, 4, 0, {0x00, 0x00, 0x00, 0x9F}},
    {0x36, 1, 0, {0x00}},
    {0x29, 0, 100, {}},
    {0x00, 0, 0, {}}
};

static void st7735_init(mp_display_obj_t *self) {
    if (self->rst != (mp_hal_pin_obj_t)-1) {
        display_reset_hw(self);
    }
    for (const st7735_init_cmd_t *entry = st7735_init_sequence; entry->cmd != 0x00; entry++) {
        if (entry->len > 0) {
            display_send_cmd_data(self, entry->cmd, entry->data, entry->len);
        } else {
            display_send_cmd(self, entry->cmd);
        }

        if (entry->delay > 0) {
            mp_hal_delay_ms(entry->delay);
        }
    }
}

static void st7735_set_rotation(mp_display_obj_t *self, uint8_t rot) {
    self->rotation = rot;
    uint8_t madctl = self->bgr ? 0x08 : 0x00;
    switch (rot) {
        case 0: madctl |= 0x00; break;
        case 1: madctl |= 0x60; break;
        case 2: madctl |= 0xC0; break;
        case 3: madctl |= 0xA0; break;
    }
    display_send_cmd_data(self, 0x36, &madctl, 1);
}

static mp_obj_t st7735_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    mp_obj_t display_obj = display_make_new_base(type, n_args, n_kw, all_args);
    mp_display_obj_t *self = MP_OBJ_TO_PTR(display_obj);
    self->set_rotation_func = st7735_set_rotation;
    st7735_init(self);
    st7735_set_rotation(self, self->rotation);
    return display_obj;
}

static const mp_rom_map_elem_t st7735_locals_dict_table[] = { };
static MP_DEFINE_CONST_DICT(st7735_locals_dict, st7735_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_st7735,
    MP_QSTR_ST7735,
    MP_TYPE_FLAG_NONE,
    make_new, st7735_make_new,
    locals_dict, &st7735_locals_dict,
    attr, display_attr,
    parent, &mp_type_display
);